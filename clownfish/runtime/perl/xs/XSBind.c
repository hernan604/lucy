/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define C_CFISH_OBJ
#define C_CFISH_VTABLE
#define C_CFISH_LOCKFREEREGISTRY
#define NEED_newRV_noinc
#include "XSBind.h"
#include "Clownfish/LockFreeRegistry.h"
#include "Clownfish/Util/StringHelper.h"
#include "Clownfish/Util/NumberUtils.h"
#include "Clownfish/Util/Memory.h"

// Convert a Perl hash into a Clownfish Hash.  Caller takes responsibility for
// a refcount.
static cfish_Hash*
S_perl_hash_to_cfish_hash(HV *phash);

// Convert a Perl array into a Clownfish VArray.  Caller takes responsibility
// for a refcount.
static cfish_VArray*
S_perl_array_to_cfish_array(AV *parray);

// Convert a VArray to a Perl array.  Caller takes responsibility for a
// refcount.
static SV*
S_cfish_array_to_perl_array(cfish_VArray *varray);

// Convert a Hash to a Perl hash.  Caller takes responsibility for a refcount.
static SV*
S_cfish_hash_to_perl_hash(cfish_Hash *hash);

cfish_Obj*
XSBind_new_blank_obj(SV *either_sv) {
    cfish_VTable *vtable;

    // Get a VTable.
    if (sv_isobject(either_sv)
        && sv_derived_from(either_sv, "Clownfish::Obj")
       ) {
        // Use the supplied object's VTable.
        IV iv_ptr = SvIV(SvRV(either_sv));
        cfish_Obj *self = INT2PTR(cfish_Obj*, iv_ptr);
        vtable = self->vtable;
    }
    else {
        // Use the supplied class name string to find a VTable.
        STRLEN len;
        char *ptr = SvPVutf8(either_sv, len);
        cfish_ZombieCharBuf *klass = CFISH_ZCB_WRAP_STR(ptr, len);
        vtable = cfish_VTable_singleton((cfish_CharBuf*)klass, NULL);
    }

    // Use the VTable to allocate a new blank object of the right size.
    return Cfish_VTable_Make_Obj(vtable);
}

cfish_Obj*
XSBind_sv_to_cfish_obj(SV *sv, cfish_VTable *vtable, void *allocation) {
    cfish_Obj *retval = XSBind_maybe_sv_to_cfish_obj(sv, vtable, allocation);
    if (!retval) {
        THROW(CFISH_ERR, "Not a %o", Cfish_VTable_Get_Name(vtable));
    }
    return retval;
}

cfish_Obj*
XSBind_maybe_sv_to_cfish_obj(SV *sv, cfish_VTable *vtable, void *allocation) {
    cfish_Obj *retval = NULL;
    if (XSBind_sv_defined(sv)) {
        if (sv_isobject(sv)
            && sv_derived_from(sv, (char*)Cfish_CB_Get_Ptr8(Cfish_VTable_Get_Name(vtable)))
           ) {
            // Unwrap a real Clownfish object.
            IV tmp = SvIV(SvRV(sv));
            retval = INT2PTR(cfish_Obj*, tmp);
        }
        else if (allocation &&
                 (vtable == CFISH_ZOMBIECHARBUF
                  || vtable == CFISH_VIEWCHARBUF
                  || vtable == CFISH_CHARBUF
                  || vtable == CFISH_OBJ)
                ) {
            // Wrap the string from an ordinary Perl scalar inside a
            // ZombieCharBuf.
            STRLEN size;
            char *ptr = SvPVutf8(sv, size);
            retval = (cfish_Obj*)cfish_ZCB_wrap_str(allocation, ptr, size);
        }
        else if (SvROK(sv)) {
            // Attempt to convert Perl hashes and arrays into their Clownfish
            // analogues.
            SV *inner = SvRV(sv);
            if (SvTYPE(inner) == SVt_PVAV && vtable == CFISH_VARRAY) {
                retval = (cfish_Obj*)S_perl_array_to_cfish_array((AV*)inner);
            }
            else if (SvTYPE(inner) == SVt_PVHV && vtable == CFISH_HASH) {
                retval = (cfish_Obj*)S_perl_hash_to_cfish_hash((HV*)inner);
            }

            if (retval) {
                // Mortalize the converted object -- which is somewhat
                // dangerous, but is the only way to avoid requiring that the
                // caller take responsibility for a refcount.
                SV *mortal = (SV*)Cfish_Obj_To_Host(retval);
                CFISH_DECREF(retval);
                sv_2mortal(mortal);
            }
        }
    }

    return retval;
}

SV*
XSBind_cfish_to_perl(cfish_Obj *obj) {
    if (obj == NULL) {
        return newSV(0);
    }
    else if (Cfish_Obj_Is_A(obj, CFISH_CHARBUF)) {
        return XSBind_cb_to_sv((cfish_CharBuf*)obj);
    }
    else if (Cfish_Obj_Is_A(obj, CFISH_BYTEBUF)) {
        return XSBind_bb_to_sv((cfish_ByteBuf*)obj);
    }
    else if (Cfish_Obj_Is_A(obj, CFISH_VARRAY)) {
        return S_cfish_array_to_perl_array((cfish_VArray*)obj);
    }
    else if (Cfish_Obj_Is_A(obj, CFISH_HASH)) {
        return S_cfish_hash_to_perl_hash((cfish_Hash*)obj);
    }
    else if (Cfish_Obj_Is_A(obj, CFISH_FLOATNUM)) {
        return newSVnv(Cfish_Obj_To_F64(obj));
    }
    else if (obj == (cfish_Obj*)CFISH_TRUE) {
        return newSViv(1);
    }
    else if (obj == (cfish_Obj*)CFISH_FALSE) {
        return newSViv(0);
    }
    else if (sizeof(IV) == 8 && Cfish_Obj_Is_A(obj, CFISH_INTNUM)) {
        int64_t num = Cfish_Obj_To_I64(obj);
        return newSViv((IV)num);
    }
    else if (sizeof(IV) == 4 && Cfish_Obj_Is_A(obj, CFISH_INTEGER32)) {
        int32_t num = (int32_t)Cfish_Obj_To_I64(obj);
        return newSViv((IV)num);
    }
    else if (sizeof(IV) == 4 && Cfish_Obj_Is_A(obj, CFISH_INTEGER64)) {
        int64_t num = Cfish_Obj_To_I64(obj);
        return newSVnv((double)num); // lossy
    }
    else {
        return (SV*)Cfish_Obj_To_Host(obj);
    }
}

cfish_Obj*
XSBind_perl_to_cfish(SV *sv) {
    cfish_Obj *retval = NULL;

    if (XSBind_sv_defined(sv)) {
        if (SvROK(sv)) {
            // Deep conversion of references.
            SV *inner = SvRV(sv);
            if (SvTYPE(inner) == SVt_PVAV) {
                retval = (cfish_Obj*)S_perl_array_to_cfish_array((AV*)inner);
            }
            else if (SvTYPE(inner) == SVt_PVHV) {
                retval = (cfish_Obj*)S_perl_hash_to_cfish_hash((HV*)inner);
            }
            else if (sv_isobject(sv)
                     && sv_derived_from(sv, "Clownfish::Obj")
                    ) {
                IV tmp = SvIV(inner);
                retval = INT2PTR(cfish_Obj*, tmp);
                (void)CFISH_INCREF(retval);
            }
        }

        // It's either a plain scalar or a non-Clownfish Perl object, so
        // stringify.
        if (!retval) {
            STRLEN len;
            char *ptr = SvPVutf8(sv, len);
            retval = (cfish_Obj*)cfish_CB_new_from_trusted_utf8(ptr, len);
        }
    }
    else if (sv) {
        // Deep conversion of raw AVs and HVs.
        if (SvTYPE(sv) == SVt_PVAV) {
            retval = (cfish_Obj*)S_perl_array_to_cfish_array((AV*)sv);
        }
        else if (SvTYPE(sv) == SVt_PVHV) {
            retval = (cfish_Obj*)S_perl_hash_to_cfish_hash((HV*)sv);
        }
    }

    return retval;
}

SV*
XSBind_bb_to_sv(const cfish_ByteBuf *bb) {
    return bb
           ? newSVpvn(Cfish_BB_Get_Buf(bb), Cfish_BB_Get_Size(bb))
           : newSV(0);
}

SV*
XSBind_cb_to_sv(const cfish_CharBuf *cb) {
    if (!cb) {
        return newSV(0);
    }
    else {
        SV *sv = newSVpvn((char*)Cfish_CB_Get_Ptr8(cb), Cfish_CB_Get_Size(cb));
        SvUTF8_on(sv);
        return sv;
    }
}

static cfish_Hash*
S_perl_hash_to_cfish_hash(HV *phash) {
    uint32_t             num_keys = hv_iterinit(phash);
    cfish_Hash          *retval   = cfish_Hash_new(num_keys);
    cfish_ZombieCharBuf *key      = CFISH_ZCB_WRAP_STR("", 0);

    while (num_keys--) {
        HE        *entry    = hv_iternext(phash);
        STRLEN     key_len  = HeKLEN(entry);
        SV        *value_sv = HeVAL(entry);
        cfish_Obj *value    = XSBind_perl_to_cfish(value_sv); // Recurse.

        // Force key to UTF-8 if necessary.
        if (key_len == (STRLEN)HEf_SVKEY) {
            // Key is stored as an SV.  Use its UTF-8 flag?  Not sure about
            // this.
            SV   *key_sv  = HeKEY_sv(entry);
            char *key_str = SvPVutf8(key_sv, key_len);
            Cfish_ZCB_Assign_Trusted_Str(key, key_str, key_len);
            Cfish_Hash_Store(retval, (cfish_Obj*)key, value);
        }
        else if (HeKUTF8(entry)) {
            Cfish_ZCB_Assign_Trusted_Str(key, HeKEY(entry), key_len);
            Cfish_Hash_Store(retval, (cfish_Obj*)key, value);
        }
        else {
            char *key_str = HeKEY(entry);
            bool pure_ascii = true;
            for (STRLEN i = 0; i < key_len; i++) {
                if ((key_str[i] & 0x80) == 0x80) { pure_ascii = false; }
            }
            if (pure_ascii) {
                Cfish_ZCB_Assign_Trusted_Str(key, key_str, key_len);
                Cfish_Hash_Store(retval, (cfish_Obj*)key, value);
            }
            else {
                SV *key_sv = HeSVKEY_force(entry);
                key_str = SvPVutf8(key_sv, key_len);
                Cfish_ZCB_Assign_Trusted_Str(key, key_str, key_len);
                Cfish_Hash_Store(retval, (cfish_Obj*)key, value);
            }
        }
    }

    return retval;
}

static cfish_VArray*
S_perl_array_to_cfish_array(AV *parray) {
    const uint32_t  size   = av_len(parray) + 1;
    cfish_VArray   *retval = cfish_VA_new(size);

    // Iterate over array elems.
    for (uint32_t i = 0; i < size; i++) {
        SV **elem_sv = av_fetch(parray, i, false);
        if (elem_sv) {
            cfish_Obj *elem = XSBind_perl_to_cfish(*elem_sv);
            if (elem) { Cfish_VA_Store(retval, i, elem); }
        }
    }
    Cfish_VA_Resize(retval, size); // needed if last elem is NULL

    return retval;
}

static SV*
S_cfish_array_to_perl_array(cfish_VArray *varray) {
    AV *perl_array = newAV();
    uint32_t num_elems = Cfish_VA_Get_Size(varray);

    // Iterate over array elems.
    if (num_elems) {
        av_fill(perl_array, num_elems - 1);
        for (uint32_t i = 0; i < num_elems; i++) {
            cfish_Obj *val = Cfish_VA_Fetch(varray, i);
            if (val == NULL) {
                continue;
            }
            else {
                // Recurse for each value.
                SV *const val_sv = XSBind_cfish_to_perl(val);
                av_store(perl_array, i, val_sv);
            }
        }
    }

    return newRV_noinc((SV*)perl_array);
}

static SV*
S_cfish_hash_to_perl_hash(cfish_Hash *hash) {
    HV *perl_hash = newHV();
    SV *key_sv    = newSV(1);
    cfish_CharBuf *key;
    cfish_Obj     *val;

    // Prepare the SV key.
    SvPOK_on(key_sv);
    SvUTF8_on(key_sv);

    // Iterate over key-value pairs.
    Cfish_Hash_Iterate(hash);
    while (Cfish_Hash_Next(hash, (cfish_Obj**)&key, &val)) {
        // Recurse for each value.
        SV *val_sv = XSBind_cfish_to_perl(val);
        if (!Cfish_Obj_Is_A((cfish_Obj*)key, CFISH_CHARBUF)) {
            CFISH_THROW(CFISH_ERR,
                        "Can't convert a key of class %o to a Perl hash key",
                        Cfish_Obj_Get_Class_Name((cfish_Obj*)key));
        }
        else {
            STRLEN key_size = Cfish_CB_Get_Size(key);
            char *key_sv_ptr = SvGROW(key_sv, key_size + 1);
            memcpy(key_sv_ptr, Cfish_CB_Get_Ptr8(key), key_size);
            SvCUR_set(key_sv, key_size);
            *SvEND(key_sv) = '\0';
            (void)hv_store_ent(perl_hash, key_sv, val_sv, 0);
        }
    }
    SvREFCNT_dec(key_sv);

    return newRV_noinc((SV*)perl_hash);
}

struct trap_context {
    SV *routine;
    SV *context;
};

static void
S_attempt_perl_call(void *context) {
    struct trap_context *args = (struct trap_context*)context;
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(newSVsv(args->context)));
    PUTBACK;
    call_sv(args->routine, G_DISCARD);
    FREETMPS;
    LEAVE;
}

cfish_Err*
XSBind_trap(SV *routine, SV *context) {
    struct trap_context args;
    args.routine = routine;
    args.context = context;
    return cfish_Err_trap(S_attempt_perl_call, &args);
}

void
XSBind_enable_overload(void *pobj) {
    SV *perl_obj = (SV*)pobj;
    HV *stash = SvSTASH(SvRV(perl_obj));
#if (PERL_VERSION > 10)
    Gv_AMupdate(stash, false);
#else
    Gv_AMupdate(stash);
#endif
    SvAMAGIC_on(perl_obj);
}

static bool
S_extract_from_sv(SV *value, void *target, const char *label,
                  bool required, int type, cfish_VTable *vtable,
                  void *allocation) {
    bool valid_assignment = false;

    if (XSBind_sv_defined(value)) {
        switch (type) {
            case XSBIND_WANT_I8:
                *((int8_t*)target) = (int8_t)SvIV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_I16:
                *((int16_t*)target) = (int16_t)SvIV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_I32:
                *((int32_t*)target) = (int32_t)SvIV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_I64:
                if (sizeof(IV) == 8) {
                    *((int64_t*)target) = (int64_t)SvIV(value);
                }
                else { // sizeof(IV) == 4
                    // lossy.
                    *((int64_t*)target) = (int64_t)SvNV(value);
                }
                valid_assignment = true;
                break;
            case XSBIND_WANT_U8:
                *((uint8_t*)target) = (uint8_t)SvUV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_U16:
                *((uint16_t*)target) = (uint16_t)SvUV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_U32:
                *((uint32_t*)target) = (uint32_t)SvUV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_U64:
                if (sizeof(UV) == 8) {
                    *((uint64_t*)target) = (uint64_t)SvUV(value);
                }
                else { // sizeof(UV) == 4
                    // lossy.
                    *((uint64_t*)target) = (uint64_t)SvNV(value);
                }
                valid_assignment = true;
                break;
            case XSBIND_WANT_BOOL:
                *((bool*)target) = !!SvTRUE(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_F32:
                *((float*)target) = (float)SvNV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_F64:
                *((double*)target) = SvNV(value);
                valid_assignment = true;
                break;
            case XSBIND_WANT_OBJ: {
                    cfish_Obj *object
                        = XSBind_maybe_sv_to_cfish_obj(value, vtable,
                                                       allocation);
                    if (object) {
                        *((cfish_Obj**)target) = object;
                        valid_assignment = true;
                    }
                    else {
                        cfish_CharBuf *mess
                            = CFISH_MAKE_MESS(
                                  "Invalid value for '%s' - not a %o",
                                  label, Cfish_VTable_Get_Name(vtable));
                        cfish_Err_set_error(cfish_Err_new(mess));
                        return false;
                    }
                }
                break;
            case XSBIND_WANT_SV:
                *((SV**)target) = value;
                valid_assignment = true;
                break;
            default: {
                    cfish_CharBuf *mess
                        = CFISH_MAKE_MESS("Unrecognized type: %i32 for param '%s'",
                                          (int32_t)type, label);
                    cfish_Err_set_error(cfish_Err_new(mess));
                    return false;
                }
        }
    }

    // Enforce that required params cannot be undef and must present valid
    // values.
    if (required && !valid_assignment) {
        cfish_CharBuf *mess = CFISH_MAKE_MESS("Missing required param %s",
                                              label);
        cfish_Err_set_error(cfish_Err_new(mess));
        return false;
    }

    return true;
}

bool
XSBind_allot_params(SV** stack, int32_t start, int32_t num_stack_elems, ...) {
    va_list args;
    size_t size = sizeof(int64_t) + num_stack_elems / 64;
    void *verified_labels = alloca(size);
    memset(verified_labels, 0, size);

    // Verify that our args come in pairs. Return success if there are no
    // args.
    if ((num_stack_elems - start) % 2 != 0) {
        cfish_CharBuf *mess
            = CFISH_MAKE_MESS(
                  "Expecting hash-style params, got odd number of args");
        cfish_Err_set_error(cfish_Err_new(mess));
        return false;
    }

    void *target;
    va_start(args, num_stack_elems);
    while (NULL != (target = va_arg(args, void*))) {
        char *label     = va_arg(args, char*);
        int   label_len = va_arg(args, int);
        int   required  = va_arg(args, int);
        int   type      = va_arg(args, int);
        cfish_VTable *vtable = va_arg(args, cfish_VTable*);
        void *allocation = va_arg(args, void*);

        // Iterate through the stack looking for labels which match this param
        // name.  If the label appears more than once, keep track of where it
        // appears *last*, as the last time a param appears overrides all
        // previous appearances.
        int32_t found_arg = -1;
        for (int32_t tick = start; tick < num_stack_elems; tick += 2) {
            SV *const key_sv = stack[tick];
            if (SvCUR(key_sv) == (STRLEN)label_len) {
                if (memcmp(SvPVX(key_sv), label, label_len) == 0) {
                    found_arg = tick;
                    lucy_NumUtil_u1set(verified_labels, tick);
                }
            }
        }

        if (found_arg == -1) {
            // Didn't find this parameter. Throw an error if it was required.
            if (required) {
                cfish_CharBuf *mess
                    = CFISH_MAKE_MESS("Missing required parameter: '%s'",
                                      label);
                cfish_Err_set_error(cfish_Err_new(mess));
                return false;
            }
        }
        else {
            // Found the arg.  Extract the value.
            SV *value = stack[found_arg + 1];
            bool got_arg = S_extract_from_sv(value, target, label,
                                                   required, type, vtable,
                                                   allocation);
            if (!got_arg) {
                CFISH_ERR_ADD_FRAME(cfish_Err_get_error());
                return false;
            }
        }
    }
    va_end(args);

    // Ensure that all parameter labels were valid.
    for (int32_t tick = start; tick < num_stack_elems; tick += 2) {
        if (!lucy_NumUtil_u1get(verified_labels, tick)) {
            SV *const key_sv = stack[tick];
            char *key = SvPV_nolen(key_sv);
            cfish_CharBuf *mess
                = CFISH_MAKE_MESS("Invalid parameter: '%s'", key);
            cfish_Err_set_error(cfish_Err_new(mess));
            return false;
        }
    }

    return true;
}

/***************************************************************************
 * The routines below are declared within the Clownfish core but left
 * unimplemented and must be defined for each host language.
 ***************************************************************************/

/**************************** Clownfish::Obj *******************************/

static void
S_lazy_init_host_obj(lucy_Obj *self) {
    SV *inner_obj = newSV(0);
    SvOBJECT_on(inner_obj);
    PL_sv_objcount++;
    SvUPGRADE(inner_obj, SVt_PVMG);
    sv_setiv(inner_obj, PTR2IV(self));

    // Connect class association.
    lucy_CharBuf *class_name = Lucy_VTable_Get_Name(self->vtable);
    HV *stash = gv_stashpvn((char*)Lucy_CB_Get_Ptr8(class_name),
                            Lucy_CB_Get_Size(class_name), TRUE);
    SvSTASH_set(inner_obj, (HV*)SvREFCNT_inc(stash));

    /* Up till now we've been keeping track of the refcount in
     * self->ref.count.  We're replacing ref.count with ref.host_obj, which
     * will assume responsibility for maintaining the refcount.  ref.host_obj
     * starts off with a refcount of 1, so we need to transfer any refcounts
     * in excess of that. */
    size_t old_refcount = self->ref.count;
    self->ref.host_obj = inner_obj;
    while (old_refcount > 1) {
        SvREFCNT_inc_simple_void_NN(inner_obj);
        old_refcount--;
    }
}

uint32_t
lucy_Obj_get_refcount(lucy_Obj *self) {
    return self->ref.count < 4
           ? self->ref.count
           : SvREFCNT((SV*)self->ref.host_obj);
}

lucy_Obj*
lucy_Obj_inc_refcount(lucy_Obj *self) {
    switch (self->ref.count) {
        case 0:
            CFISH_THROW(LUCY_ERR, "Illegal refcount of 0");
            break; // useless
        case 1:
        case 2:
            self->ref.count++;
            break;
        case 3:
            S_lazy_init_host_obj(self);
            // fall through
        default:
            SvREFCNT_inc_simple_void_NN((SV*)self->ref.host_obj);
    }
    return self;
}

uint32_t
lucy_Obj_dec_refcount(lucy_Obj *self) {
    uint32_t modified_refcount = INT32_MAX;
    switch (self->ref.count) {
        case 0:
            CFISH_THROW(LUCY_ERR, "Illegal refcount of 0");
            break; // useless
        case 1:
            modified_refcount = 0;
            Lucy_Obj_Destroy(self);
            break;
        case 2:
        case 3:
            modified_refcount = --self->ref.count;
            break;
        default:
            modified_refcount = SvREFCNT((SV*)self->ref.host_obj) - 1;
            // If the SV's refcount falls to 0, DESTROY will be invoked from
            // Perl-space.
            SvREFCNT_dec((SV*)self->ref.host_obj);
    }
    return modified_refcount;
}

void*
lucy_Obj_to_host(lucy_Obj *self) {
    if (self->ref.count < 4) { S_lazy_init_host_obj(self); }
    return newRV_inc((SV*)self->ref.host_obj);
}

/*************************** Clownfish::VTable ******************************/

lucy_Obj*
lucy_VTable_foster_obj(lucy_VTable *self, void *host_obj) {
    lucy_Obj *obj
        = (lucy_Obj*)lucy_Memory_wrapped_calloc(self->obj_alloc_size, 1);
    SV *inner_obj = SvRV((SV*)host_obj);
    obj->vtable = self;
    sv_setiv(inner_obj, PTR2IV(obj));
    obj->ref.host_obj = inner_obj;
    return obj;
}

void
lucy_VTable_register_with_host(lucy_VTable *singleton, lucy_VTable *parent) {
    dSP;
    ENTER;
    SAVETMPS;
    EXTEND(SP, 2);
    PUSHMARK(SP);
    mPUSHs((SV*)Lucy_VTable_To_Host(singleton));
    mPUSHs((SV*)Lucy_VTable_To_Host(parent));
    PUTBACK;
    call_pv("Clownfish::VTable::_register", G_VOID | G_DISCARD);
    FREETMPS;
    LEAVE;
}

lucy_VArray*
lucy_VTable_fresh_host_methods(const lucy_CharBuf *class_name) {
    dSP;
    ENTER;
    SAVETMPS;
    EXTEND(SP, 1);
    PUSHMARK(SP);
    mPUSHs(XSBind_cb_to_sv(class_name));
    PUTBACK;
    call_pv("Clownfish::VTable::_fresh_host_methods", G_SCALAR);
    SPAGAIN;
    cfish_VArray *methods = (cfish_VArray*)XSBind_perl_to_cfish(POPs);
    PUTBACK;
    FREETMPS;
    LEAVE;
    return methods;
}

lucy_CharBuf*
lucy_VTable_find_parent_class(const lucy_CharBuf *class_name) {
    dSP;
    ENTER;
    SAVETMPS;
    EXTEND(SP, 1);
    PUSHMARK(SP);
    mPUSHs(XSBind_cb_to_sv(class_name));
    PUTBACK;
    call_pv("Clownfish::VTable::_find_parent_class", G_SCALAR);
    SPAGAIN;
    SV *parent_class_sv = POPs;
    PUTBACK;
    cfish_CharBuf *parent_class
        = (cfish_CharBuf*)XSBind_perl_to_cfish(parent_class_sv);
    FREETMPS;
    LEAVE;
    return parent_class;
}

void*
lucy_VTable_to_host(lucy_VTable *self) {
    bool first_time = self->ref.count < 4 ? true : false;
    Lucy_VTable_To_Host_t to_host
        = CFISH_SUPER_METHOD_PTR(LUCY_VTABLE, Lucy_VTable_To_Host);
    SV *host_obj = (SV*)to_host(self);
    if (first_time) {
        SvSHARE((SV*)self->ref.host_obj);
    }
    return host_obj;
}


/***************************** Clownfish::Err *******************************/

// Anonymous XSUB helper for Err#trap().  It wraps the supplied C function
// so that it can be run inside a Perl eval block.
static SV *attempt_xsub = NULL;

XS(lucy_Err_attempt_via_xs) {
    dXSARGS;
    CHY_UNUSED_VAR(cv);
    SP -= items;
    if (items != 2) {
        CFISH_THROW(CFISH_ERR, "Usage: $sub->(routine, context)");
    };
    IV routine_iv = SvIV(ST(0));
    IV context_iv = SvIV(ST(1));
    Cfish_Err_Attempt_t routine = INT2PTR(Cfish_Err_Attempt_t, routine_iv);
    void *context               = INT2PTR(void*, context_iv);
    routine(context);
    XSRETURN(0);
}

void
lucy_Err_init_class(void) {
    char *file = (char*)__FILE__;
    attempt_xsub = (SV*)newXS(NULL, lucy_Err_attempt_via_xs, file);
}

lucy_Err*
lucy_Err_get_error() {
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    PUTBACK;
    call_pv("Clownfish::Err::get_error", G_SCALAR);
    SPAGAIN;
    cfish_Err *error = (cfish_Err*)XSBind_perl_to_cfish(POPs);
    PUTBACK;
    FREETMPS;
    LEAVE;
    return error;
}

void
lucy_Err_set_error(lucy_Err *error) {
    dSP;
    ENTER;
    SAVETMPS;
    EXTEND(SP, 2);
    PUSHMARK(SP);
    PUSHmortal;
    if (error) {
        mPUSHs((SV*)Lucy_Err_To_Host(error));
    }
    else {
        PUSHmortal;
    }
    PUTBACK;
    call_pv("Clownfish::Err::set_error", G_VOID | G_DISCARD);
    FREETMPS;
    LEAVE;
}

void
lucy_Err_do_throw(lucy_Err *err) {
    dSP;
    SV *error_sv = (SV*)Lucy_Err_To_Host(err);
    CFISH_DECREF(err);
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    XPUSHs(sv_2mortal(error_sv));
    PUTBACK;
    call_pv("Clownfish::Err::do_throw", G_DISCARD);
    FREETMPS;
    LEAVE;
}

void*
lucy_Err_to_host(lucy_Err *self) {
    Lucy_Err_To_Host_t super_to_host
        = CFISH_SUPER_METHOD_PTR(LUCY_ERR, Lucy_Err_To_Host);
    SV *perl_obj = (SV*)super_to_host(self);
    XSBind_enable_overload(perl_obj);
    return perl_obj;
}

void
lucy_Err_throw_mess(lucy_VTable *vtable, lucy_CharBuf *message) {
    Lucy_Err_Make_t make
        = CFISH_METHOD_PTR(CFISH_CERTIFY(vtable, LUCY_VTABLE), Lucy_Err_Make);
    lucy_Err *err = (lucy_Err*)CFISH_CERTIFY(make(NULL), LUCY_ERR);
    Lucy_Err_Cat_Mess(err, message);
    CFISH_DECREF(message);
    lucy_Err_do_throw(err);
}

void
lucy_Err_warn_mess(lucy_CharBuf *message) {
    SV *error_sv = XSBind_cb_to_sv(message);
    CFISH_DECREF(message);
    warn("%s", SvPV_nolen(error_sv));
    SvREFCNT_dec(error_sv);
}

lucy_Err*
lucy_Err_trap(Cfish_Err_Attempt_t routine, void *context) {
    lucy_Err *error = NULL;
    SV *routine_sv = newSViv(PTR2IV(routine));
    SV *context_sv = newSViv(PTR2IV(context));
    dSP;
    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    EXTEND(SP, 2);
    PUSHs(sv_2mortal(routine_sv));
    PUSHs(sv_2mortal(context_sv));
    PUTBACK;

    int count = call_sv(attempt_xsub, G_EVAL | G_DISCARD);
    if (count != 0) {
        lucy_CharBuf *mess
            = lucy_CB_newf("'attempt' returned too many values: %i32",
                           (int32_t)count);
        error = lucy_Err_new(mess);
    }
    else {
        SV *dollar_at = get_sv("@", FALSE);
        if (SvTRUE(dollar_at)) {
            if (sv_isobject(dollar_at)
                && sv_derived_from(dollar_at,"Clownfish::Err")
               ) {
                IV error_iv = SvIV(SvRV(dollar_at));
                error = INT2PTR(lucy_Err*, error_iv);
                CFISH_INCREF(error);
            }
            else {
                STRLEN len;
                char *ptr = SvPVutf8(dollar_at, len);
                lucy_CharBuf *mess = lucy_CB_new_from_trusted_utf8(ptr, len);
                error = lucy_Err_new(mess);
            }
        }
    }
    FREETMPS;
    LEAVE;

    return error;
}

/*********************** Clownfish::LockFreeRegistry ************************/

void*
lucy_LFReg_to_host(lucy_LockFreeRegistry *self) {
    bool first_time = self->ref.count < 4 ? true : false;
    Lucy_LFReg_To_Host_t to_host
        = CFISH_SUPER_METHOD_PTR(LUCY_LOCKFREEREGISTRY, Lucy_LFReg_To_Host);
    SV *host_obj = (SV*)to_host(self);
    if (first_time) {
        SvSHARE((SV*)self->ref.host_obj);
    }
    return host_obj;
}


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

#define C_LUCY_BLOBTYPE
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Plan/BlobType.h"

BlobType*
BlobType_new(bool stored) {
    BlobType *self = (BlobType*)VTable_Make_Obj(BLOBTYPE);
    return BlobType_init(self, stored);
}

BlobType*
BlobType_init(BlobType *self, bool stored) {
    FType_init((FieldType*)self);
    BlobTypeIVARS *const ivars = BlobType_IVARS(self);
    ivars->stored = stored;
    return self;
}

bool
BlobType_Binary_IMP(BlobType *self) {
    UNUSED_VAR(self);
    return true;
}

void
BlobType_Set_Sortable_IMP(BlobType *self, bool sortable) {
    UNUSED_VAR(self);
    if (sortable) { THROW(ERR, "BlobType fields can't be sortable"); }
}

ViewByteBuf*
BlobType_Make_Blank_IMP(BlobType *self) {
    UNUSED_VAR(self);
    return ViewBB_new(NULL, 0);
}

int8_t
BlobType_Primitive_ID_IMP(BlobType *self) {
    UNUSED_VAR(self);
    return FType_BLOB;
}

bool
BlobType_Equals_IMP(BlobType *self, Obj *other) {
    if ((BlobType*)other == self)   { return true; }
    if (!Obj_Is_A(other, BLOBTYPE)) { return false; }
    BlobType_Equals_t super_equals
        = (BlobType_Equals_t)SUPER_METHOD_PTR(BLOBTYPE, LUCY_BlobType_Equals);
    return super_equals(self, other);
}

Hash*
BlobType_Dump_For_Schema_IMP(BlobType *self) {
    BlobTypeIVARS *const ivars = BlobType_IVARS(self);
    Hash *dump = Hash_new(0);
    Hash_Store_Str(dump, "type", 4, (Obj*)CB_newf("blob"));

    // Store attributes that override the defaults -- even if they're
    // meaningless.
    if (ivars->boost != 1.0) {
        Hash_Store_Str(dump, "boost", 5, (Obj*)CB_newf("%f64", ivars->boost));
    }
    if (ivars->indexed) {
        Hash_Store_Str(dump, "indexed", 7, (Obj*)CFISH_TRUE);
    }
    if (ivars->stored) {
        Hash_Store_Str(dump, "stored", 6, (Obj*)CFISH_TRUE);
    }

    return dump;
}

Hash*
BlobType_Dump_IMP(BlobType *self) {
    Hash *dump = BlobType_Dump_For_Schema(self);
    Hash_Store_Str(dump, "_class", 6,
                   (Obj*)CB_Clone(BlobType_Get_Class_Name(self)));
    DECREF(Hash_Delete_Str(dump, "type", 4));
    return dump;
}

BlobType*
BlobType_Load_IMP(BlobType *self, Obj *dump) {
    Hash *source = (Hash*)CERTIFY(dump, HASH);
    CharBuf *class_name = (CharBuf*)Hash_Fetch_Str(source, "_class", 6);
    VTable *vtable
        = (class_name != NULL && Obj_Is_A((Obj*)class_name, CHARBUF))
          ? VTable_singleton(class_name, NULL)
          : BLOBTYPE;
    BlobType *loaded     = (BlobType*)VTable_Make_Obj(vtable);
    Obj *boost_dump      = Hash_Fetch_Str(source, "boost", 5);
    Obj *indexed_dump    = Hash_Fetch_Str(source, "indexed", 7);
    Obj *stored_dump     = Hash_Fetch_Str(source, "stored", 6);
    UNUSED_VAR(self);

    BlobType_init(loaded, false);
    BlobTypeIVARS *const loaded_ivars = BlobType_IVARS(loaded);
    if (boost_dump) {
        loaded_ivars->boost = (float)Obj_To_F64(boost_dump);
    }
    if (indexed_dump) {
        loaded_ivars->indexed = Obj_To_Bool(indexed_dump);
    }
    if (stored_dump){
        loaded_ivars->stored = Obj_To_Bool(stored_dump);
    }

    return loaded;
}



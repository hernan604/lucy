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
#define CFISH_USE_SHORT_NAMES
#define CHY_USE_SHORT_NAMES

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Clownfish/Obj.h"
#include "Clownfish/CharBuf.h"
#include "Clownfish/Err.h"
#include "Clownfish/Hash.h"
#include "Clownfish/VTable.h"
#include "Clownfish/Util/Memory.h"

Obj*
Obj_init(Obj *self) {
    ABSTRACT_CLASS_CHECK(self, OBJ);
    return self;
}

void
Obj_Destroy_IMP(Obj *self) {
    FREEMEM(self);
}

int32_t
Obj_Hash_Sum_IMP(Obj *self) {
    int64_t hash_sum = PTR_TO_I64(self);
    return (int32_t)hash_sum;
}

bool
Obj_Is_A_IMP(Obj *self, VTable *ancestor) {
    VTable *vtable = self ? self->vtable : NULL;

    while (vtable != NULL) {
        if (vtable == ancestor) {
            return true;
        }
        vtable = vtable->parent;
    }

    return false;
}

bool
Obj_Equals_IMP(Obj *self, Obj *other) {
    return (self == other);
}

CharBuf*
Obj_To_String_IMP(Obj *self) {
#if (SIZEOF_PTR == 4)
    return CB_newf("%o@0x%x32", Obj_Get_Class_Name(self), self);
#elif (SIZEOF_PTR == 8)
    int64_t   iaddress   = PTR_TO_I64(self);
    uint64_t  address    = (uint64_t)iaddress;
    uint32_t  address_hi = address >> 32;
    uint32_t  address_lo = address & 0xFFFFFFFF;
    return CB_newf("%o@0x%x32%x32", Obj_Get_Class_Name(self), address_hi,
                   address_lo);
#else
  #error "Unexpected pointer size."
#endif
}

bool
Obj_To_Bool_IMP(Obj *self) {
    return !!Obj_To_I64(self);
}

VTable*
Obj_Get_VTable_IMP(Obj *self) {
    return self->vtable;
}

CharBuf*
Obj_Get_Class_Name_IMP(Obj *self) {
    return VTable_Get_Name(self->vtable);
}



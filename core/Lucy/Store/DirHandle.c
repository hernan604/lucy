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

#define C_LUCY_DIRHANDLE
#include "Lucy/Util/ToolSet.h"
#include "Lucy/Store/DirHandle.h"

DirHandle*
DH_init(DirHandle *self, const CharBuf *dir) {
    DirHandleIVARS *const ivars = DH_IVARS(self);
    ivars->dir   = CB_Clone(dir);
    ivars->entry = CB_new(32);
    ABSTRACT_CLASS_CHECK(self, DIRHANDLE);
    return self;
}

void
DH_destroy(DirHandle *self) {
    DirHandleIVARS *const ivars = DH_IVARS(self);
    DH_Close(self);
    DECREF(ivars->dir);
    DECREF(ivars->entry);
    SUPER_DESTROY(self, DIRHANDLE);
}

CharBuf*
DH_get_dir(DirHandle *self) {
    return DH_IVARS(self)->dir;
}

CharBuf*
DH_get_entry(DirHandle *self) {
    return DH_IVARS(self)->entry;
}



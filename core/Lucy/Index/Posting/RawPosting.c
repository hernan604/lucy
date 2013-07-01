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

#define C_LUCY_RAWPOSTING
#define C_LUCY_RAWPOSTINGWRITER
#define C_LUCY_TERMINFO
#include "Lucy/Util/ToolSet.h"

#include <string.h>

#include "Lucy/Index/Posting/RawPosting.h"
#include "Lucy/Index/PolyReader.h"
#include "Lucy/Index/Segment.h"
#include "Lucy/Index/Snapshot.h"
#include "Lucy/Index/TermInfo.h"
#include "Lucy/Plan/Schema.h"
#include "Lucy/Store/OutStream.h"
#include "Clownfish/Util/StringHelper.h"

RawPosting*
RawPost_new(void *pre_allocated_memory, int32_t doc_id, uint32_t freq,
            char *term_text, size_t term_text_len) {
    RawPosting *self
        = (RawPosting*)VTable_Init_Obj(RAWPOSTING, pre_allocated_memory);
    RawPostingIVARS *const ivars = RawPost_IVARS(self);
    ivars->doc_id      = doc_id;
    ivars->freq        = freq;
    ivars->content_len = term_text_len;
    ivars->aux_len     = 0;
    memcpy(&ivars->blob, term_text, term_text_len);

    return self;
}

void
RawPost_destroy(RawPosting *self) {
    UNUSED_VAR(self);
    THROW(ERR, "Illegal attempt to destroy RawPosting object");
}

uint32_t
RawPost_get_refcount(RawPosting* self) {
    UNUSED_VAR(self);
    return 1;
}

RawPosting*
RawPost_inc_refcount(RawPosting* self) {
    return self;
}

uint32_t
RawPost_dec_refcount(RawPosting* self) {
    UNUSED_VAR(self);
    return 1;
}

/***************************************************************************/

RawPostingWriter*
RawPostWriter_new(Schema *schema, Snapshot *snapshot, Segment *segment,
                  PolyReader *polyreader, OutStream *outstream) {
    RawPostingWriter *self
        = (RawPostingWriter*)VTable_Make_Obj(RAWPOSTINGWRITER);
    return RawPostWriter_init(self, schema, snapshot, segment, polyreader,
                              outstream);
}

RawPostingWriter*
RawPostWriter_init(RawPostingWriter *self, Schema *schema,
                   Snapshot *snapshot, Segment *segment,
                   PolyReader *polyreader, OutStream *outstream) {
    const int32_t invalid_field_num = 0;
    PostWriter_init((PostingWriter*)self, schema, snapshot, segment,
                    polyreader, invalid_field_num);
    RawPostingWriterIVARS *const ivars = RawPostWriter_IVARS(self);
    ivars->outstream = (OutStream*)INCREF(outstream);
    ivars->last_doc_id = 0;
    return self;
}

void
RawPostWriter_start_term(RawPostingWriter *self, TermInfo *tinfo) {
    RawPostingWriterIVARS *const ivars = RawPostWriter_IVARS(self);
    ivars->last_doc_id   = 0;
    tinfo->post_filepos = OutStream_Tell(ivars->outstream);
}

void
RawPostWriter_update_skip_info(RawPostingWriter *self, TermInfo *tinfo) {
    RawPostingWriterIVARS *const ivars = RawPostWriter_IVARS(self);
    TermInfoIVARS *const tinfo_ivars = TInfo_IVARS(tinfo);
    tinfo_ivars->post_filepos = OutStream_Tell(ivars->outstream);
}

void
RawPostWriter_destroy(RawPostingWriter *self) {
    RawPostingWriterIVARS *const ivars = RawPostWriter_IVARS(self);
    DECREF(ivars->outstream);
    SUPER_DESTROY(self, RAWPOSTINGWRITER);
}

void
RawPostWriter_write_posting(RawPostingWriter *self, RawPosting *posting) {
    RawPostingWriterIVARS *const ivars = RawPostWriter_IVARS(self);
    RawPostingIVARS *const posting_ivars = RawPost_IVARS(posting);
    OutStream *const outstream   = ivars->outstream;
    const int32_t    doc_id      = posting_ivars->doc_id;
    const uint32_t   delta_doc   = doc_id - ivars->last_doc_id;
    char  *const     aux_content = posting_ivars->blob
                                   + posting_ivars->content_len;
    if (posting->freq == 1) {
        const uint32_t doc_code = (delta_doc << 1) | 1;
        OutStream_Write_C32(outstream, doc_code);
    }
    else {
        const uint32_t doc_code = delta_doc << 1;
        OutStream_Write_C32(outstream, doc_code);
        OutStream_Write_C32(outstream, posting->freq);
    }
    OutStream_Write_Bytes(outstream, aux_content, posting_ivars->aux_len);
    ivars->last_doc_id = doc_id;
}



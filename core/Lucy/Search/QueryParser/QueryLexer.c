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

#define C_LUCY_QUERYLEXER
#include <stdlib.h>
#include <ctype.h>
#include "Lucy/Util/ToolSet.h"

#include "Lucy/Search/QueryParser/QueryLexer.h"
#include "Lucy/Search/QueryParser.h"
#include "Lucy/Search/QueryParser/ParserElem.h"

#define TOKEN_OPEN_PAREN  LUCY_QPARSER_TOKEN_OPEN_PAREN
#define TOKEN_CLOSE_PAREN LUCY_QPARSER_TOKEN_CLOSE_PAREN
#define TOKEN_MINUS       LUCY_QPARSER_TOKEN_MINUS
#define TOKEN_PLUS        LUCY_QPARSER_TOKEN_PLUS
#define TOKEN_NOT         LUCY_QPARSER_TOKEN_NOT
#define TOKEN_OR          LUCY_QPARSER_TOKEN_OR
#define TOKEN_AND         LUCY_QPARSER_TOKEN_AND
#define TOKEN_FIELD       LUCY_QPARSER_TOKEN_FIELD
#define TOKEN_STRING      LUCY_QPARSER_TOKEN_STRING
#define TOKEN_QUERY       LUCY_QPARSER_TOKEN_QUERY

static ParserElem*
S_consume_keyword(ZombieCharBuf *qstring, const char *keyword,
                  size_t keyword_len, int type);

static ParserElem*
S_consume_field(ZombieCharBuf *qstring);

static ParserElem*
S_consume_text(ZombieCharBuf *qstring);

static ParserElem*
S_consume_quoted_string(ZombieCharBuf *qstring);

QueryLexer*
QueryLexer_new() {
    QueryLexer *self = (QueryLexer*)VTable_Make_Obj(QUERYLEXER);
    return QueryLexer_init(self);
}

QueryLexer*
QueryLexer_init(QueryLexer *self) {
    QueryLexerIVARS *const ivars = QueryLexer_IVARS(self);
    ivars->heed_colons = false;
    return self;
}

bool
QueryLexer_Heed_Colons_IMP(QueryLexer *self) {
    return QueryLexer_IVARS(self)->heed_colons;
}

void
QueryLexer_Set_Heed_Colons_IMP(QueryLexer *self, bool heed_colons) {
    QueryLexer_IVARS(self)->heed_colons = heed_colons;
}

VArray*
QueryLexer_Tokenize_IMP(QueryLexer *self, const CharBuf *query_string) {
    QueryLexerIVARS *const ivars = QueryLexer_IVARS(self);
    CharBuf *copy = query_string
                    ? CB_Clone(query_string)
                    : CB_new_from_trusted_utf8("", 0);
    ZombieCharBuf *qstring = ZCB_WRAP((CharBuf*)copy);
    VArray *elems = VA_new(0);
    ZCB_Trim(qstring);

    while (ZCB_Get_Size(qstring)) {
        ParserElem *elem = NULL;

        if (ZCB_Trim_Top(qstring)) {
            // Fast-forward past whitespace.
            continue;
        }

        if (ivars->heed_colons) {
            ParserElem *elem = S_consume_field(qstring);
            if (elem) {
                VA_Push(elems, (Obj*)elem);
            }
        }

        uint32_t code_point = ZCB_Code_Point_At(qstring, 0);
        switch (code_point) {
            case '(':
                ZCB_Nip(qstring, 1);
                elem = ParserElem_new(TOKEN_OPEN_PAREN, NULL);
                break;
            case ')':
                ZCB_Nip(qstring, 1);
                elem = ParserElem_new(TOKEN_CLOSE_PAREN, NULL);
                break;
            case '+':
                if (ZCB_Get_Size(qstring) > 1
                    && !StrHelp_is_whitespace(ZCB_Code_Point_At(qstring, 1))
                   ) {
                    elem = ParserElem_new(TOKEN_PLUS, NULL);
                }
                else {
                    elem = ParserElem_new(TOKEN_STRING, (Obj*)CB_newf("+"));
                }
                ZCB_Nip(qstring, 1);
                break;
            case '-':
                if (ZCB_Get_Size(qstring) > 1
                    && !StrHelp_is_whitespace(ZCB_Code_Point_At(qstring, 1))
                   ) {
                    elem = ParserElem_new(TOKEN_MINUS, NULL);
                }
                else {
                    elem = ParserElem_new(TOKEN_STRING, (Obj*)CB_newf("-"));
                }
                ZCB_Nip(qstring, 1);
                break;
            case '"':
                elem = S_consume_quoted_string(qstring);
                break;
            case 'O':
                elem = S_consume_keyword(qstring, "OR", 2, TOKEN_OR);
                if (!elem) {
                    elem = S_consume_text(qstring);
                }
                break;
            case 'A':
                elem = S_consume_keyword(qstring, "AND", 3, TOKEN_AND);
                if (!elem) {
                    elem = S_consume_text(qstring);
                }
                break;
            case 'N':
                elem = S_consume_keyword(qstring, "NOT", 3, TOKEN_NOT);
                if (!elem) {
                    elem = S_consume_text(qstring);
                }
                break;
            default:
                elem = S_consume_text(qstring);
                break;
        }
        VA_Push(elems, (Obj*)elem);
    }

    DECREF(copy);
    return elems;
}


static ParserElem*
S_consume_keyword(ZombieCharBuf *qstring, const char *keyword,
                  size_t keyword_len, int type) {
    if (!ZCB_Starts_With_Str(qstring, keyword, keyword_len)) {
        return NULL;
    }
    uint32_t lookahead = ZCB_Code_Point_At(qstring, keyword_len);
    if (!lookahead) {
        return NULL;
    }
    if (StrHelp_is_whitespace(lookahead)
        || lookahead == '"'
        || lookahead == '('
        || lookahead == ')'
        || lookahead == '+'
        || lookahead == '-'
       ) {
        ZCB_Nip(qstring, keyword_len);
        return ParserElem_new(type, NULL);
    }
    return NULL;
}

static ParserElem*
S_consume_field(ZombieCharBuf *qstring) {
    size_t tick = 0;

    // Field names constructs must start with a letter or underscore.
    uint32_t code_point = ZCB_Code_Point_At(qstring, tick);
    if (isalpha(code_point) || code_point == '_') {
        tick++;
    }
    else {
        return NULL;
    }

    // Only alphanumerics and underscores are allowed  in field names.
    while (1) {
        code_point = ZCB_Code_Point_At(qstring, tick);
        if (isalnum(code_point) || code_point == '_') {
            tick++;
        }
        else if (code_point == ':') {
            tick++;
            break;
        }
        else {
            return NULL;
        }
    }

    // Field name constructs must be followed by something sensible.
    uint32_t lookahead = ZCB_Code_Point_At(qstring, tick);
    if (!(isalnum(lookahead)
          || lookahead == '_'
          || lookahead > 127
          || lookahead == '"'
          || lookahead == '('
         )
       ) {
        return NULL;
    }

    // Consume string data.
    ZombieCharBuf *field = ZCB_WRAP((CharBuf*)qstring);
    ZCB_Truncate(field, tick - 1);
    ZCB_Nip(qstring, tick);
    return ParserElem_new(TOKEN_FIELD, (Obj*)ZCB_Clone(field));
}

static ParserElem*
S_consume_text(ZombieCharBuf *qstring) {
    ZombieCharBuf *text  = ZCB_WRAP((CharBuf*)qstring);
    size_t tick = 0;
    while (1) {
        uint32_t code_point = ZCB_Nibble(qstring);
        if (code_point == '\\') {
            code_point = ZCB_Nibble(qstring);
            tick++;
            if (code_point == 0) {
                break;
            }
        }
        else if (StrHelp_is_whitespace(code_point)
            || code_point == '"'
            || code_point == '('
            || code_point == ')'
            || code_point == 0 
           ) {
            break;
        }
        tick++;
    }

    ZCB_Truncate(text, tick);
    return ParserElem_new(TOKEN_STRING, (Obj*)ZCB_Clone(text));
}

static ParserElem*
S_consume_quoted_string(ZombieCharBuf *qstring) {
    ZombieCharBuf *text = ZCB_WRAP((CharBuf*)qstring);
    if (ZCB_Nibble(qstring) != '"') {
        THROW(ERR, "Internal error: expected a quote");
    }

    size_t tick = 1;
    while (1) {
        uint32_t code_point = ZCB_Nibble(qstring);
        if (code_point == '"') {
            tick += 1;
            break;
        }
        else if (code_point == 0) {
            break;
        }
        else if (code_point == '\\') {
            ZCB_Nibble(qstring);
            tick += 2;
        }
        else {
            tick += 1;
        }
    }

    ZCB_Truncate(text, tick);
    return ParserElem_new(TOKEN_STRING, (Obj*)ZCB_Clone(text));
}


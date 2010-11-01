# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

package KinoSearch::Object::Num;
use KinoSearch;

1;

__END__

__BINDING__

my $float32_xs_code = <<'END_XS_CODE';
MODULE = KinoSearch   PACKAGE = KinoSearch::Object::Float32

SV*
new(either_sv, value)
    SV    *either_sv;
    float  value;
CODE:
{
    kino_Float32 *self = (kino_Float32*)XSBind_new_blank_obj(either_sv);
    kino_Float32_init(self, value);
    RETVAL = CFISH_OBJ_TO_SV_NOINC(self);
}
OUTPUT: RETVAL
END_XS_CODE

my $float64_xs_code = <<'END_XS_CODE';
MODULE = KinoSearch   PACKAGE = KinoSearch::Object::Float64

SV*
new(either_sv, value)
    SV     *either_sv;
    double  value;
CODE:
{
    kino_Float64 *self = (kino_Float64*)XSBind_new_blank_obj(either_sv);
    kino_Float64_init(self, value);
    RETVAL = CFISH_OBJ_TO_SV_NOINC(self);
}
OUTPUT: RETVAL
END_XS_CODE

Clownfish::Binding::Perl::Class->register(
    parcel       => "KinoSearch",
    class_name   => "KinoSearch::Object::Float32",
    xs_code      => $float32_xs_code,
    bind_methods => [qw( Set_Value Get_Value )],
);
Clownfish::Binding::Perl::Class->register(
    parcel       => "KinoSearch",
    class_name   => "KinoSearch::Object::Float64",
    xs_code      => $float64_xs_code,
    bind_methods => [qw( Set_Value Get_Value )],
);



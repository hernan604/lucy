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
package Lucy::Build::Binding::Store;

sub bind_all {
    my $class = shift;
    $class->bind_fsfilehandle;
    $class->bind_fsfolder;
    $class->bind_filehandle;
    $class->bind_folder;
    $class->bind_instream;
    $class->bind_lock;
    $class->bind_lockfilelock;
    $class->bind_sharedlock;
    $class->bind_lockerr;
    $class->bind_lockfactory;
    $class->bind_outstream;
    $class->bind_ramfile;
    $class->bind_ramfilehandle;
    $class->bind_ramfolder;
}

sub bind_fsfilehandle {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::FSFileHandle",
    );
    $binding->bind_constructor( alias => '_open', initializer => 'do_open' );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_fsfolder {
    my $synopsis = <<'END_SYNOPSIS';
    my $folder = Lucy::Store::FSFolder->new(
        path   => '/path/to/folder',
    );
END_SYNOPSIS

    my $constructor = $synopsis;

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::FSFolder",
        make_pod          => {
            synopsis    => $synopsis,
            constructor => { sample => $constructor },
        },
    );
    $binding->bind_constructor;
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_filehandle {
    my $xs_code = <<'END_XS_CODE';
MODULE = Lucy     PACKAGE = Lucy::Store::FileHandle

=for comment

For testing purposes only.  Track number of FileHandle objects in existence.

=cut

uint32_t
FH_READ_ONLY()
CODE:
    RETVAL = LUCY_FH_READ_ONLY;
OUTPUT: RETVAL

uint32_t
FH_WRITE_ONLY()
CODE:
    RETVAL = LUCY_FH_WRITE_ONLY;
OUTPUT: RETVAL

uint32_t
FH_CREATE()
CODE:
    RETVAL = LUCY_FH_CREATE;
OUTPUT: RETVAL

uint32_t
FH_EXCLUSIVE()
CODE:
    RETVAL = LUCY_FH_EXCLUSIVE;
OUTPUT: RETVAL


int32_t
object_count()
CODE:
    RETVAL = lucy_FH_object_count;
OUTPUT: RETVAL

=for comment

For testing purposes only.  Used to help produce buffer alignment tests.

=cut

IV
_BUF_SIZE()
CODE:
   RETVAL = LUCY_IO_STREAM_BUF_SIZE;
OUTPUT: RETVAL
END_XS_CODE

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::FileHandle",
        xs_code           => $xs_code,
    );
    $binding->bind_constructor( alias => '_open', initializer => 'do_open' );
    $binding->bind_method( method => $_ ) for qw( Length Close );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_folder {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel       => "Lucy",
        class_name   => "Lucy::Store::Folder",
        make_pod          => { synopsis => "    # Abstract base class.\n", },
    );
    $binding->bind_constructor;
    $binding->bind_method( method => $_ ) for qw(
        Open_Out
        Open_In
        MkDir
        List_R
        Exists
        Rename
        Hard_Link
        Delete
        Slurp_File
        Close
        Get_Path
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_instream {
    my $xs_code = <<'END_XS_CODE';
MODULE = Lucy    PACKAGE = Lucy::Store::InStream

void
read(self, buffer_sv, len, ...)
    lucy_InStream *self;
    SV *buffer_sv;
    size_t len;
PPCODE:
{
    UV offset = items == 4 ? SvUV(ST(3)) : 0;
    char *ptr;
    size_t total_len = offset + len;
    SvUPGRADE(buffer_sv, SVt_PV);
    if (!SvPOK(buffer_sv)) { SvCUR_set(buffer_sv, 0); }
    ptr = SvGROW(buffer_sv, total_len + 1);
    Lucy_InStream_Read_Bytes(self, ptr + offset, len);
    SvPOK_on(buffer_sv);
    if (SvCUR(buffer_sv) < total_len) {
        SvCUR_set(buffer_sv, total_len);
        *(SvEND(buffer_sv)) = '\0';
    }
}

SV*
read_string(self)
    lucy_InStream *self;
CODE:
{
    char *ptr;
    size_t len = Lucy_InStream_Read_C32(self);
    RETVAL = newSV(len + 1);
    SvCUR_set(RETVAL, len);
    SvPOK_on(RETVAL);
    SvUTF8_on(RETVAL); // Trust source.  Reconsider if API goes public.
    *SvEND(RETVAL) = '\0';
    ptr = SvPVX(RETVAL);
    Lucy_InStream_Read_Bytes(self, ptr, len);
}
OUTPUT: RETVAL

int
read_raw_c64(self, buffer_sv)
    lucy_InStream *self;
    SV *buffer_sv;
CODE:
{
    char *ptr;
    SvUPGRADE(buffer_sv, SVt_PV);
    ptr = SvGROW(buffer_sv, 10 + 1);
    RETVAL = Lucy_InStream_Read_Raw_C64(self, ptr);
    SvPOK_on(buffer_sv);
    SvCUR_set(buffer_sv, RETVAL);
}
OUTPUT: RETVAL
END_XS_CODE

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel       => "Lucy",
        class_name   => "Lucy::Store::InStream",
        xs_code      => $xs_code,
    );
    $binding->bind_constructor( alias => 'open', initializer => 'do_open' );
    $binding->bind_method( method => $_ ) for qw(
        Seek
        Tell
        Length
        Reopen
        Close
        Read_I8
        Read_I32
        Read_I64
        Read_U8
        Read_U32
        Read_U64
        Read_C32
        Read_C64
        Read_F32
        Read_F64
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_lock {
    my $synopsis = <<'END_SYNOPSIS';
    my $lock = $lock_factory->make_lock(
        name    => 'write',
        timeout => 5000,
    );
    $lock->obtain or die "can't get lock for " . $lock->get_name;
    do_stuff();
    $lock->release;
END_SYNOPSIS

    my $constructor = <<'END_CONSTRUCTOR';
    my $lock = Lucy::Store::Lock->new(
        name     => 'commit',     # required
        folder   => $folder,      # required
        host     => $hostname,    # required
        timeout  => 5000,         # default: 0
        interval => 1000,         # default: 100
    );
END_CONSTRUCTOR

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel       => "Lucy",
        class_name   => "Lucy::Store::Lock",
        make_pod          => {
            synopsis    => $synopsis,
            constructor => { sample => $constructor },
            methods     => [
                qw(
                    obtain
                    request
                    release
                    is_locked
                    clear_stale
                    )
            ],
        },
    );
    $binding->bind_constructor;
    $binding->bind_method( method => $_ ) for qw(
        Obtain
        Request
        Is_Locked
        Release
        Clear_Stale
        Get_Name
        Get_Lock_Path
        Get_Host
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_lockfilelock {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::LockFileLock",
    );
    $binding->bind_constructor;
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_sharedlock {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::SharedLock",
    );
    $binding->bind_constructor;
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_lockerr {
    my $synopsis = <<'END_SYNOPSIS';
    while (1) {
        my $bg_merger = eval {
            Lucy::Index::BackgroundMerger->new( index => $index );
        };
        if ( blessed($@) and $@->isa("Lucy::Store::LockErr") ) {
            warn "Retrying...\n";
        }
        elsif (!$bg_merger) {
            # Re-throw.
            die "Failed to open BackgroundMerger: $@";
        }
        ...
    }
END_SYNOPSIS

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel     => "Lucy",
        class_name => "Lucy::Store::LockErr",
        make_pod   => { synopsis => $synopsis }
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_lockfactory {
    my $synopsis = <<'END_SYNOPSIS';
    use Sys::Hostname qw( hostname );
    my $hostname = hostname() or die "Can't get unique hostname";
    my $folder = Lucy::Store::FSFolder->new( 
        path => '/path/to/index', 
    );
    my $lock_factory = Lucy::Store::LockFactory->new(
        folder => $folder,
        host   => $hostname,
    );
    my $write_lock = $lock_factory->make_lock(
        name     => 'write',
        timeout  => 5000,
        interval => 100,
    );
END_SYNOPSIS

    my $constructor = <<'END_CONSTRUCTOR';
    my $lock_factory = Lucy::Store::LockFactory->new(
        folder => $folder,      # required
        host   => $hostname,    # required
    );
END_CONSTRUCTOR

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::LockFactory",
        make_pod          => {
            methods     => [qw( make_lock make_shared_lock)],
            synopsis    => $synopsis,
            constructor => { sample => $constructor },
        }
    );
    $binding->bind_constructor;
    $binding->bind_method( method => $_ ) for qw(
        Make_Lock
        Make_Shared_Lock
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_outstream {
    my $xs_code = <<'END_XS_CODE';
MODULE = Lucy     PACKAGE = Lucy::Store::OutStream

void
print(self, ...)
    lucy_OutStream *self;
PPCODE:
{
    int i;
    for (i = 1; i < items; i++) {
        STRLEN len;
        char *ptr = SvPV(ST(i), len);
        Lucy_OutStream_Write_Bytes(self, ptr, len);
    }
}

void
write_string(self, aSV)
    lucy_OutStream *self;
    SV *aSV;
PPCODE:
{
    STRLEN len = 0;
    char *ptr = SvPVutf8(aSV, len);
    Lucy_OutStream_Write_C32(self, len);
    Lucy_OutStream_Write_Bytes(self, ptr, len);
}
END_XS_CODE

    my $synopsis = <<'END_SYNOPSIS';    # Don't use this yet.
    my $outstream = $folder->open_out($filename) or die $@;
    $outstream->write_u64($file_position);
END_SYNOPSIS

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel       => "Lucy",
        class_name   => "Lucy::Store::OutStream",
        xs_code      => $xs_code,
    );
    $binding->bind_constructor( alias => 'open', initializer => 'do_open' );
    $binding->bind_method( method => $_ ) for qw(
        Tell
        Length
        Flush
        Close
        Absorb
        Write_I8
        Write_I32
        Write_I64
        Write_U8
        Write_U32
        Write_U64
        Write_C32
        Write_C64
        Write_F32
        Write_F64
    );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_ramfile {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::RAMFile",
    );
    $binding->bind_constructor;
    $binding->bind_method( method => $_ ) for qw( Get_Contents );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_ramfilehandle {
    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::RAMFileHandle",
    );
    $binding->bind_constructor( alias => '_open', initializer => 'do_open' );
    $binding->bind_method( method => $_ ) for qw( Get_File );
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

sub bind_ramfolder {
    my $synopsis = <<'END_SYNOPSIS';
    my $folder = Lucy::Store::RAMFolder->new;
    
    # or sometimes...
    my $folder = Lucy::Store::RAMFolder->new(
        path => $relative_path,
    );
END_SYNOPSIS

    my $constructor = <<'END_CONSTRUCTOR';
    my $folder = Lucy::Store::RAMFolder->new(
        path => $relative_path,   # default: empty string
    );
END_CONSTRUCTOR

    my $binding = Clownfish::CFC::Binding::Perl::Class->new(
        parcel            => "Lucy",
        class_name        => "Lucy::Store::RAMFolder",
        make_pod          => {
            synopsis    => $synopsis,
            constructor => { sample => $constructor },
        }
    );
    $binding->bind_constructor;
    Clownfish::CFC::Binding::Perl::Class->register($binding);
}

1;
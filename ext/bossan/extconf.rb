require 'mkmf'

$CFLAGS << " -std=gnu99 -fPIC"

$DLDFLAGS << " -fPIC"

bossan_dir = File.expand_path("../", __FILE__)

Dir.chdir bossan_dir

dir_config bossan_dir

create_makefile "bossan/bossan_ext"

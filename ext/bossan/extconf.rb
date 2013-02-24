require 'mkmf'

$CFLAGS << " -std=gnu99 -fPIC"

$DLDFLAGS << " -fPIC"

bossan_dir = File.expand_path("../", __FILE__)

Dir.chdir bossan_dir

dir_config bossan_dir

srcs = Dir.glob("*.c")
if have_header("sys/epoll.h")
  srcs.delete("picoev_kqueue.c")
elsif have_header("sys/event.h")
  srcs.delete("picoev_epoll.c")
else
  $stderr.puts "error: not found kqueue or epoll on your system."
  exit 1
end
$srcs = srcs

create_makefile "bossan/bossan_ext"

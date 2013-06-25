require 'mkmf'

$CPPFLAGS="`pkg-config --cflags libimobiledevice-1.0`"
$LDFLAGS="`pkg-config --libs libimobiledevice-1.0`"
pkg_config("libimobiledevice-1.0")

create_makefile("io/afc/afc_base")

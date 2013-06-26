require 'mkmf'

$CPPFLAGS="`pkg-config --cflags libimobiledevice-1.0`"
$LDFLAGS="`pkg-config --libs libimobiledevice-1.0`"
pkg_config("libimobiledevice-1.0")
have_header("libimobiledevice/house_arrest.h")
have_func("lockdownd_service_descriptor_free")
have_func("lockdownd_get_device_udid")

create_makefile("io/afc/afc_base")

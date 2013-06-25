//-*- coding: utf-8
//-*- vim: sw=4:ts=4:sts=4

#include "ioafc.h"
#include <ruby/intern.h>
#include <errno.h>
#include <sys/fcntl.h>



namespace {
    VALUE rb_cIoAFC = rb_define_class_under(rb_cIO, "AFCBase", rb_cObject);
    VALUE rb_cIoAFCFD = rb_define_class_under(rb_cIoAFC, "Descriptor", rb_cObject);
    VALUE rb_eNoDevice = rb_define_class_under(rb_cIoAFC, "NoDevice", rb_eRuntimeError);
    VALUE rb_eProtected = rb_define_class_under(rb_cIoAFC, "PasswordProtected", rb_eRuntimeError);
    VALUE rb_eConnectFailed = rb_define_class_under(rb_cIoAFC, "ConnectFailed", rb_eRuntimeError);
}

static void free_dictionary(char** dict)
{
    if (dict) {
        for (int i = 0; dict[i]; ++i) {
            ::free(dict[i]);
        }
        ::free(dict);
    }
}

static afc_file_mode_t get_afc_file_mode(int flags)
{
	switch (flags & O_ACCMODE) {
		case O_RDONLY:
            return AFC_FOPEN_RDONLY;
            
		case O_WRONLY:
			if ((flags & O_TRUNC) == O_TRUNC) return AFC_FOPEN_WRONLY;
            if ((flags & O_APPEND) == O_APPEND) return AFC_FOPEN_APPEND;
            return AFC_FOPEN_RW;
            
		case O_RDWR:
			if ((flags & O_TRUNC) == O_TRUNC) return AFC_FOPEN_WR;
            if ((flags & O_APPEND) == O_APPEND) return AFC_FOPEN_RDAPPEND;
            return AFC_FOPEN_RW;
	}
    rb_raise(rb_eArgError, "Invalid open mode(%d)", flags);
}


static void rb_raise_SystemCallError(int err)
{
    VALUE e = INT2FIX(err);
    rb_exc_raise(rb_class_new_instance(1, &e, rb_eSystemCallError));
}


static void rb_raise_SystemCallErrorAFC(afc_error_t err)
{
    struct AFC_ERRMAP {
        afc_error_t afc;
        int         sys;
    };
    static const AFC_ERRMAP map[] = {
        {AFC_E_SUCCESS              , 0},
        {AFC_E_OP_HEADER_INVALID    , EIO},
        {AFC_E_NO_RESOURCES         , EMFILE},
        {AFC_E_READ_ERROR           , ENOTDIR},
        {AFC_E_WRITE_ERROR          , EIO},
        {AFC_E_UNKNOWN_PACKET_TYPE  , EIO},
        {AFC_E_INVALID_ARG          , EINVAL},
        {AFC_E_OBJECT_NOT_FOUND     , ENOENT},
        {AFC_E_OBJECT_IS_DIR        , EISDIR},
        {AFC_E_DIR_NOT_EMPTY        , ENOTEMPTY},
        {AFC_E_PERM_DENIED          , EPERM},
        {AFC_E_SERVICE_NOT_CONNECTED, ENXIO},
        {AFC_E_OP_TIMEOUT           , ETIMEDOUT},
        {AFC_E_TOO_MUCH_DATA        , EFBIG},
        {AFC_E_END_OF_DATA          , ENODATA},
        {AFC_E_OP_NOT_SUPPORTED     , ENOSYS},
        {AFC_E_OBJECT_EXISTS        , EEXIST},
        {AFC_E_OBJECT_BUSY          , EBUSY},
        {AFC_E_NO_SPACE_LEFT        , ENOSPC},
        {AFC_E_OP_WOULD_BLOCK       , EWOULDBLOCK},
        {AFC_E_IO_ERROR             , EIO},
        {AFC_E_OP_INTERRUPTED       , EINTR},
        {AFC_E_OP_IN_PROGRESS       , EALREADY},
        {AFC_E_INTERNAL_ERROR       , EIO},
        {-1}
    };
    for (const AFC_ERRMAP* p = map; p->afc>=0; ++p) {
        if (err==(p->afc)) {
            rb_raise_SystemCallError(p->sys);
        }
    }
    rb_raise_SystemCallError(EIO);
}


static inline void rb_raise_SystemCallErrorAFC_IF(afc_error_t err)
{
    if (err!=AFC_E_SUCCESS) rb_raise_SystemCallErrorAFC(err);
}


static VALUE rb_array_from_dictionary(char** dict)
{
    VALUE result = rb_ary_new();
    for (int i=0; dict[i]; ++i) {
        rb_ary_unshift(result, rb_str_new2(dict[i]));
    }
    free_dictionary(dict);
    return result;
}



#pragma mark - IoAFC


IoAFC::IoAFC()
{
    _blocksize = 4096;
    _idev = NULL;
    _control = NULL;
    _house_arrest = NULL;
    _port = NULL;
    _afc = NULL;
}


IoAFC::~IoAFC()
{
    try{
        teardown();
    } catch(...){}
}


void IoAFC::teardown()
{
    DBG("- IO::AFC: %p\n", _afc);
    if (_afc) afc_client_free(_afc);
    if (_port) lockdownd_service_descriptor_free(_port);
    if (_house_arrest) house_arrest_client_free(_house_arrest);
    if (_control) lockdownd_client_free(_control);
    if (_idev) idevice_free(_idev);
    _afc = NULL;
    _port = NULL;
    _house_arrest = NULL;
    _control = NULL;
    _idev = NULL;
}


void IoAFC::init(VALUE rb_device_uuid, VALUE rb_appid)
{
    const char* device_uuid = NULL;
    const char* appid = NULL;
    if (!NIL_P(rb_device_uuid)) {
        device_uuid = rb_string_value_cstr(&rb_device_uuid);
    }
    if (!NIL_P(rb_appid)) {
        appid = rb_string_value_cstr(&rb_appid);
    }
    
    if (device_uuid) {
        _device_uuid.assign(device_uuid);
    } else {
        _device_uuid.erase();
    }
    if (appid) {
        _service_name = HOUSE_ARREST_SERVICE_NAME;
        _appid.assign(appid);
    } else {
        _service_name = AFC_SERVICE_NAME;
        _appid.erase();
    }
    
    idevice_new(&_idev, device_uuid);
    if (!_idev) {
        teardown();
        rb_raise(rb_eNoDevice, "No device found.");
    }
    
    lockdownd_error_t le = lockdownd_client_new_with_handshake(_idev, &_control, "Ruby::IoAFC");
    if (le!=LOCKDOWN_E_SUCCESS) {
        teardown();
        if (le==LOCKDOWN_E_PASSWORD_PROTECTED) {
            rb_raise(rb_eProtected, "Password protected.");
        }
        rb_raise(rb_eConnectFailed, "Failed to connect to lockdownd servce.");
    }
    
    le = lockdownd_start_service(_control, _service_name, &_port);
    if ((le!=LOCKDOWN_E_SUCCESS) || (_port==0)) {
        teardown();
        rb_raise(rb_eConnectFailed, "Failed to start %s service.", _service_name);
    }
    
    if (appid) {
        // check document foider?
        house_arrest_client_new(_idev, _port, &_house_arrest);
        if (!_house_arrest) {
            teardown();
            rb_raise(rb_eConnectFailed, "Failed to start document sharing service.");
        }
        
        house_arrest_error_t he = house_arrest_send_command(_house_arrest, "VendContainer", appid);
        if (he!=HOUSE_ARREST_E_SUCCESS) {
            teardown();
            rb_raise(rb_eConnectFailed, "Failed to start document sharing service.");
        }
        
        plist_t dict = NULL;
        he = house_arrest_get_result(_house_arrest, &dict);
        if (he!=HOUSE_ARREST_E_SUCCESS) {
            teardown();
            rb_raise(rb_eConnectFailed, "Failed to get result from document sharing service.");
        }
        plist_t node = plist_dict_get_item(dict, "Error");
        if (node) {
            char *str = NULL;
            plist_get_string_val(node, &str);
            if (str) {
                _errmsg.assign(str);
            } else {
                _errmsg.assign("(null)");
            }
            ::free(str);
            plist_free(dict);
            teardown();
            rb_raise(rb_eConnectFailed, "Error: %s", _errmsg.c_str());
        }
        plist_free(dict);
    }
    
    if (_house_arrest) {
        afc_client_new_from_house_arrest_client(_house_arrest, &_afc);
    } else {
        afc_client_new(_idev, _port, &_afc);
    }
    
    if (!_afc) {
        teardown();
        rb_raise(rb_eConnectFailed, "Failed to connect AFC.");
    }
    
    char** info_raw = NULL;
    afc_error_t ae = afc_get_device_info(_afc, &info_raw);
    if ((ae!=AFC_E_SUCCESS) || (info_raw==NULL)) {
        teardown();
        rb_raise(rb_eConnectFailed, "Failed to get device info.");
    }
    for (int i=0; info_raw[i]; i+=2) {
        if (strcmp(info_raw[i],"FSBlockSize")!=0) {
            _blocksize = atoi(info_raw[i+1]);
            break;
        }
    }
    free_dictionary(info_raw);
    DBG("+ IO::AFC: %p\n", _afc);
}


uint64_t IoAFC::open(afc_client_t* afc, VALUE rb_path, VALUE rb_mode)
{
    checkHandle();
    
    uint64_t handle = 0;
    afc_file_mode_t mode = get_afc_file_mode(NUM2INT(rb_mode));
    afc_error_t ae = afc_file_open(_afc, rb_string_value_cstr(&rb_path), mode, &handle);
    rb_raise_SystemCallErrorAFC_IF(ae);
    *afc = _afc;
    return handle;
}


VALUE IoAFC::test_closed()
{
    return (_afc==NULL)? Qtrue: Qfalse;
}


VALUE IoAFC::close()
{
    teardown();
    return Qnil;
}


VALUE IoAFC::getattr(VALUE rb_path)
{
    checkHandle();
    
    char** info = NULL;
    afc_error_t ae = afc_get_file_info(_afc, rb_string_value_cstr(&rb_path), &info);
    rb_raise_SystemCallErrorAFC_IF(ae);
    if (info==NULL) {
        rb_raise_SystemCallError(ENOENT);
    }
    return rb_array_from_dictionary(info);
}



VALUE IoAFC::readdir(VALUE rb_path)
{
    checkHandle();
    
    char** dirs = NULL;
    afc_error_t ae = afc_read_directory(_afc, rb_string_value_cstr(&rb_path), &dirs);
    rb_raise_SystemCallErrorAFC_IF(ae);
    if (dirs==NULL) {
        rb_raise_SystemCallError(ENOENT);
    }
    return rb_array_from_dictionary(dirs);
}



VALUE IoAFC::utimens(VALUE rb_path, VALUE rb_time)
{
    checkHandle();
    
    struct timespec ts = rb_time_timespec(rb_time);
    uint64_t mt = (uint64_t)ts.tv_sec * (uint64_t)1000000000 + (uint64_t)ts.tv_nsec;
    
    afc_error_t ae = afc_set_file_time(_afc, rb_string_value_cstr(&rb_path), mt);
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



VALUE IoAFC::statfs()
{
    checkHandle();
    
    char** info= NULL;
    afc_error_t ae = afc_get_device_info(_afc, &info);
    rb_raise_SystemCallErrorAFC_IF(ae);
    if (info==NULL) {
        rb_raise_SystemCallError(ENOENT);
    }
    return rb_array_from_dictionary(info);
}



VALUE IoAFC::truncate(VALUE rb_path, VALUE rb_size)
{
    checkHandle();
    
    afc_error_t ae = afc_truncate(_afc, rb_string_value_cstr(&rb_path), NUM2ULL(rb_size));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return rb_size;
}



// afc.symlink("hello", "/tmp/world")
VALUE IoAFC::symlink(VALUE rb_target, VALUE rb_linkname)
{
    checkHandle();
    
    afc_error_t ae = afc_make_link(_afc, AFC_SYMLINK, rb_string_value_cstr(&rb_target), rb_string_value_cstr(&rb_linkname));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



// afc.link("/tmp/hello", "/tmp/world") #=> ln /tmp/hello /tmp/world
VALUE IoAFC::link(VALUE rb_target, VALUE rb_linkname)
{
    checkHandle();
    
    afc_error_t ae = afc_make_link(_afc, AFC_HARDLINK, rb_string_value_cstr(&rb_target), rb_string_value_cstr(&rb_linkname));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



VALUE IoAFC::unlink(VALUE rb_path)
{
    checkHandle();
    
    afc_error_t ae = afc_remove_path(_afc, rb_string_value_cstr(&rb_path));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



VALUE IoAFC::rename(VALUE rb_from, VALUE rb_to)
{
    checkHandle();
    
    afc_error_t ae = afc_rename_path(_afc, rb_string_value_cstr(&rb_from), rb_string_value_cstr(&rb_to));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



VALUE IoAFC::mkdir(VALUE rb_path)
{
    checkHandle();
    
    afc_error_t ae = afc_make_directory(_afc, rb_string_value_cstr(&rb_path));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return Qtrue;
}



VALUE IoAFC::get_device_udid()
{
    checkHandle();
    
    char* udid = NULL;
    lockdownd_error_t le = lockdownd_get_device_udid(_control, &udid);
    if (le!=LOCKDOWN_E_SUCCESS) {
        rb_raise(rb_eConnectFailed, "lockdownd_get_device_udid() failed - %d", le);
    }
    VALUE result = Qnil;
    if (udid) {
        result = rb_str_new2(udid);
        ::free(udid);
    }
    return result;
}



VALUE IoAFC::get_evice_display_name()
{
    checkHandle();
    
    char* name = NULL;
    lockdownd_error_t le = lockdownd_get_device_name(_control, &name);
    if (le!=LOCKDOWN_E_SUCCESS) {
        rb_raise(rb_eConnectFailed, "lockdownd_get_device_name() failed - %d", le);
    }
    VALUE result = Qnil;
    if (name) {
        result = rb_str_new2(name);
        ::free(name);
    }
    return result;
}



static VALUE plist_string_to_rb_string(plist_t node)
{
    char* s = NULL;
    plist_get_string_val(node, &s);
    if (s==NULL) {
        return Qnil;
    }
    VALUE v = rb_str_new2(s);
    ::free(s);
    return v;
}

VALUE IoAFC::enum_applications()
{
    checkHandle();
    
    lockdownd_service_descriptor_t port = NULL;
    lockdownd_error_t le = lockdownd_start_service(_control, INSTALLATION_PROXY_SERVICE_NAME, &port);
    if ((le!=LOCKDOWN_E_SUCCESS) || (port==0)) {
        rb_raise(rb_eConnectFailed, "Failed to start %s service.", INSTALLATION_PROXY_SERVICE_NAME);
    }
    
    instproxy_client_t client = NULL;
    instproxy_error_t ie = instproxy_client_new(_idev, port, &client);
    if ((ie!=INSTPROXY_E_SUCCESS) || (client==NULL)) {
        lockdownd_service_descriptor_free(port);
        rb_raise(rb_eRuntimeError, "instproxy_client_new() failed - %d", ie);
    }
    
    plist_t client_opts = instproxy_client_options_new();
    if (client_opts==NULL) {
        instproxy_client_free(client);
        lockdownd_service_descriptor_free(port);
        rb_raise(rb_eRuntimeError, "instproxy_client_options_new() failed");
    }
    
    plist_t apps = NULL;
    instproxy_client_options_add(client_opts, "ApplicationType", "User", NULL);
    ie = instproxy_browse(client, client_opts, &apps);
    instproxy_client_options_free(client_opts);
    instproxy_client_free(client);
    lockdownd_service_descriptor_free(port);
    if (ie!=INSTPROXY_E_SUCCESS) {
        rb_raise(rb_eRuntimeError, "instproxy_browse() failed - %d", ie);
    }
    
    VALUE result = rb_hash_new();
    if (apps!=NULL) {
        uint32_t num = plist_array_get_size(apps);
        VALUE rb_displayName = rb_str_new2("CFBundleDisplayName");
        VALUE rb_sharing = rb_str_new2("UIFileSharingEnabled");
        
        for (uint32_t i=0; i<num; ++i) {
            plist_t app = plist_array_get_item(apps, i);
            plist_t id = plist_dict_get_item(app, "CFBundleIdentifier");
            plist_t name = plist_dict_get_item(app, "CFBundleDisplayName");
//            plist_t type = plist_dict_get_item(app, "CFBundleDocumentTypes");
//            if (plist_array_get_size(type)==0) {
//                type = NULL;
//            }
            plist_t sharing = plist_dict_get_item(app, "UIFileSharingEnabled");
            uint8_t b_sharing = false;
            if (sharing) {
                plist_type t = plist_get_node_type(sharing);
                if (t==PLIST_BOOLEAN) {
                    plist_get_bool_val(sharing, &b_sharing);
                } else if (t==PLIST_STRING) {
                    char* v = NULL;
                    plist_get_string_val(sharing, &v);
                    if ((strcmp(v, "YES")==0) || (strcmp(v, "true")==0)) {
                        b_sharing = true;
                    }
                    ::free(v);
                }
            }
            
            VALUE h = rb_hash_new();
            rb_hash_aset(h, rb_displayName, plist_string_to_rb_string(name));
            rb_hash_aset(h, rb_sharing, (b_sharing)? Qtrue: Qfalse);
            rb_hash_aset(result, plist_string_to_rb_string(id), h);
        }
        plist_free(apps);
    }
    
    return result;
}




#pragma mark - IoAFCDescriptor

IoAFCDescriptor::IoAFCDescriptor()
{
    _afc = NULL;
    _handle = 0;
}



IoAFCDescriptor::~IoAFCDescriptor()
{
    try{
        teardown();
    } catch(...){}
}



void IoAFCDescriptor::teardown()
{
    if (_afc) {
        DBG("- IO::AFC::FD: %p, %llx\n", _afc, _handle);
        afc_file_close(_afc, _handle);
        _afc = NULL;
        _handle = 0;
    }
}



void IoAFCDescriptor::init(VALUE rb_afc, VALUE rb_path, VALUE rb_mode)
{
    if (NIL_P(rb_afc)) {
        rb_raise(rb_eRuntimeError, "Invalid AFC object.");
    }
    _handle = IoAFC::rb_open(rb_afc, &_afc, rb_path, rb_mode);
    DBG("+ IO::AFC::FD: %p, %llx\n", _afc, _handle);
}



VALUE IoAFCDescriptor::test_closed()
{
    return (_handle==0)? Qtrue: Qfalse;
}



VALUE IoAFCDescriptor::close()
{
    teardown();
    return Qnil;
}



#define MAX_READ_DATA_LENGTH    (8192)
VALUE IoAFCDescriptor::read(VALUE rb_size)
{
    checkHandle();
    
    uint32_t size = (uint32_t)NUM2ULONG(rb_size);
    if (size==0) {
        return Qnil;
    }
    if (size>MAX_READ_DATA_LENGTH) {
        size = MAX_READ_DATA_LENGTH;
    }
    
    char data[MAX_READ_DATA_LENGTH];
    afc_error_t ae = afc_file_read(_afc, _handle, data, size, &size);
    rb_raise_SystemCallErrorAFC_IF(ae);
    return rb_str_new(data, size);
}



#define MAX_WRITE_DATA_LENGTH   (65536*16)
VALUE IoAFCDescriptor::write(VALUE rb_data)
{
    checkHandle();

    if (NIL_P(rb_data)) {
        return INT2FIX(0);
    }    
    const char* data = rb_string_value_ptr(&rb_data);
    off_t size = NUM2ULL(rb_str_length(rb_data));
    if (size==0) {
        return INT2FIX(0);
    }
    
    off_t wrote = 0;
    while (size>0) {
        uint32_t length = (size>MAX_WRITE_DATA_LENGTH)? MAX_WRITE_DATA_LENGTH: (uint32_t)size;
        afc_error_t ae = afc_file_write(_afc, _handle, data, length, &length);
        rb_raise_SystemCallErrorAFC_IF(ae);
        if (length==0) {
            rb_raise(rb_eRuntimeError, "Unexpected error: No data is written.");
        }
        data += length;
        size -= length;
        wrote += length;
    }
    
    return ULL2NUM(wrote);
}



VALUE IoAFCDescriptor::seek(VALUE rb_offset, VALUE rb_mode)
{
    checkHandle();
    
    afc_error_t ae = afc_file_seek(_afc, _handle, NUM2ULL(rb_offset), NUM2INT(rb_mode));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return tell();
}



VALUE IoAFCDescriptor::tell()
{
    checkHandle();
    
    uint64_t pos = 0;
    afc_error_t ae = afc_file_tell(_afc, _handle, &pos);
    rb_raise_SystemCallErrorAFC_IF(ae);
    return ULL2NUM(pos);
}



VALUE IoAFCDescriptor::ftruncate(VALUE rb_size)
{
    checkHandle();
    
    afc_error_t ae = afc_file_truncate(_afc, _handle, NUM2ULL(rb_size));
    rb_raise_SystemCallErrorAFC_IF(ae);
    return rb_size;
}




#pragma mark - Ruby method definition.

void IoAFC::define_class()
{
    rb_define_alloc_func(rb_cIoAFC, (rb_alloc_func_t)rb_alloc);
    rb_define_method(rb_cIoAFC, "initialize", RUBY_METHOD_FUNC(rb_init), 2);
    rb_define_method(rb_cIoAFC, "closed?", RUBY_METHOD_FUNC(rb_test_closed), 0);
    rb_define_method(rb_cIoAFC, "close", RUBY_METHOD_FUNC(rb_close), 0);
    rb_define_method(rb_cIoAFC, "getattr", RUBY_METHOD_FUNC(rb_getattr), 1);
    rb_define_method(rb_cIoAFC, "readdir", RUBY_METHOD_FUNC(rb_readdir), 1);
    rb_define_method(rb_cIoAFC, "utimens", RUBY_METHOD_FUNC(rb_utimens), 2);
    rb_define_method(rb_cIoAFC, "statfs", RUBY_METHOD_FUNC(rb_statfs), 0);
    rb_define_method(rb_cIoAFC, "truncate", RUBY_METHOD_FUNC(rb_truncate), 2);
    rb_define_method(rb_cIoAFC, "symlink", RUBY_METHOD_FUNC(rb_symlink), 2);
    rb_define_method(rb_cIoAFC, "link", RUBY_METHOD_FUNC(rb_link), 2);
    rb_define_method(rb_cIoAFC, "unlink", RUBY_METHOD_FUNC(rb_unlink), 1);
    rb_define_method(rb_cIoAFC, "rename", RUBY_METHOD_FUNC(rb_rename), 2);
    rb_define_method(rb_cIoAFC, "mkdir", RUBY_METHOD_FUNC(rb_mkdir), 1);
    
    rb_define_method(rb_cIoAFC, "device_udid", RUBY_METHOD_FUNC(rb_get_device_udid), 0);
    rb_define_method(rb_cIoAFC, "device_display_name", RUBY_METHOD_FUNC(rb_get_device_display_name), 0);
    rb_define_method(rb_cIoAFC, "applications", RUBY_METHOD_FUNC(rb_enum_applications), 0);
}



void IoAFCDescriptor::define_class()
{
    rb_define_alloc_func(rb_cIoAFCFD, (rb_alloc_func_t)rb_alloc);
    rb_define_method(rb_cIoAFCFD, "initialize", RUBY_METHOD_FUNC(rb_init), 3);
    rb_define_method(rb_cIoAFCFD, "closed?", RUBY_METHOD_FUNC(rb_test_closed), 0);
    rb_define_method(rb_cIoAFCFD, "close", RUBY_METHOD_FUNC(rb_close), 0);
    rb_define_method(rb_cIoAFCFD, "read", RUBY_METHOD_FUNC(rb_read), 1);
    rb_define_method(rb_cIoAFCFD, "write", RUBY_METHOD_FUNC(rb_write), 1);
    rb_define_method(rb_cIoAFCFD, "seek", RUBY_METHOD_FUNC(rb_seek), 2);
    rb_define_method(rb_cIoAFCFD, "tell", RUBY_METHOD_FUNC(rb_tell), 0);
    rb_define_method(rb_cIoAFCFD, "ftruncate", RUBY_METHOD_FUNC(rb_ftruncate), 1);
}




extern "C" {
    void Init_afc_base();
}
void Init_afc_base()
{
    IoAFC::define_class();
    IoAFCDescriptor::define_class();
}

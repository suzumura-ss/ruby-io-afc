//-*- coding: utf-8
//-*- vim: sw=4:ts=4:sts=4

#ifndef __INCLUDE_IO_AFC_H__
#define __INCLUDE_IO_AFC_H__
#pragma GCC visibility push(default)

#include "ruby_wrapper.h"
#include <string>
#include <vector>

// https://github.com/mcolyer/ifuse/blob/master/src/ifuse.c
#define AFC_SERVICE_NAME "com.apple.afc"
#define HOUSE_ARREST_SERVICE_NAME "com.apple.mobile.house_arrest"

// https://github.com/GNOME/gvfs/blob/master/daemon/gvfsbackendafc.c
#define INSTALLATION_PROXY_SERVICE_NAME "com.apple.mobile.installation_proxy"

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>
#include <libimobiledevice/afc.h>
#include <libimobiledevice/house_arrest.h>
#include <libimobiledevice/installation_proxy.h>


#define DBG(...)    //printf(__VA_ARGS__)

using namespace std;


class IoAFC : public RubyWrapper<IoAFC>
{
protected:
    off_t               _blocksize; //= 4096
    idevice_t           _idev;
    lockdownd_client_t  _control;
    lockdownd_service_descriptor_t _port;
    afc_client_t        _afc;
    house_arrest_client_t _house_arrest;

    string  _device_uuid;
    string  _appid;
    const char* _service_name;
    string  _errmsg;

public:
    IoAFC();
    virtual ~IoAFC();

protected:
    inline void checkHandle()
    {
        if (_afc==NULL) rb_raise(rb_eIOError, "closed handle.");
    }

    void teardown();
    void init(VALUE rb_device_uuid, VALUE rb_appid);
    VALUE test_closed();
    VALUE close();
    uint64_t open(afc_client_t* afc, VALUE rb_path, VALUE rb_mode);
    VALUE getattr(VALUE rb_path);
    VALUE readdir(VALUE rb_path);
    VALUE utimens(VALUE rb_path, VALUE rb_time);
    VALUE statfs();
    VALUE truncate(VALUE rb_path, VALUE rb_size);
    VALUE symlink(VALUE rb_target, VALUE rb_linkname);
    VALUE link(VALUE rb_target, VALUE rb_linkname);
    VALUE unlink(VALUE rb_path);
    VALUE rename(VALUE rb_from, VALUE rb_to);
    VALUE mkdir(VALUE rb_path);
    VALUE get_device_udid();
    VALUE get_evice_display_name();
    VALUE enum_applications();
    
public:
    static void define_class();
    static VALUE rb_init(VALUE self, VALUE rb_device_uuid, VALUE rb_appid)
    {
        get_self(self)->init(rb_device_uuid, rb_appid);
        return self;
    }
    
    static VALUE rb_test_closed(VALUE self)
    {
        return get_self(self)->test_closed();
    }
    
    static VALUE rb_close(VALUE self)
    {
        return get_self(self)->close();
    }
    
    static uint64_t rb_open(VALUE self, afc_client_t* afc, VALUE rb_path, VALUE rb_mode)
    {
        return get_self(self)->open(afc, rb_path, rb_mode);
    }
    
    static VALUE rb_getattr(VALUE self, VALUE rb_path)
    {
        return get_self(self)->getattr(rb_path);
    }
    
    static VALUE rb_readdir(VALUE self, VALUE rb_path)
    {
        return get_self(self)->readdir(rb_path);
    }
    
    static VALUE rb_utimens(VALUE self, VALUE rb_path, VALUE rb_time)
    {
        return get_self(self)->utimens(rb_path, rb_time);
    }
    
    static VALUE rb_statfs(VALUE self)
    {
        return get_self(self)->statfs();
    }
    
    static VALUE rb_truncate(VALUE self, VALUE rb_path, VALUE rb_size)
    {
        return get_self(self)->truncate(rb_path, rb_size);
    }
    
    static VALUE rb_symlink(VALUE self, VALUE rb_target, VALUE rb_linkname)
    {
        return get_self(self)->symlink(rb_target, rb_linkname);
    }
    
    static VALUE rb_link(VALUE self, VALUE rb_target, VALUE rb_linkname)
    {
        return get_self(self)->link(rb_target, rb_linkname);
    }
    
    static VALUE rb_unlink(VALUE self, VALUE rb_path)
    {
        return get_self(self)->unlink(rb_path);
    }
    
    static VALUE rb_rename(VALUE self, VALUE rb_from, VALUE rb_to)
    {
        return get_self(self)->rename(rb_from, rb_to);
    }
    
    static VALUE rb_mkdir(VALUE self, VALUE rb_path)
    {
        return get_self(self)->mkdir(rb_path);
    }
    
    static VALUE rb_get_device_udid(VALUE self)
    {
        return get_self(self)->get_device_udid();
    }
    
    static VALUE rb_get_device_display_name(VALUE self)
    {
        return get_self(self)->get_evice_display_name();
    }
    
    static VALUE rb_enum_applications(VALUE self)
    {
        return get_self(self)->enum_applications();
    }
};



class IoAFCDescriptor : public RubyWrapper<IoAFCDescriptor>
{
protected:
    afc_client_t _afc;
    uint64_t _handle;
    uint64_t _seekptr;
    
public:
    IoAFCDescriptor();
    virtual ~IoAFCDescriptor();
    
protected:
    inline void checkHandle()
    {
        if (_afc==NULL) rb_raise(rb_eIOError, "closed handle.");
    }
    
    void teardown();
    void init(VALUE rb_afc, VALUE rb_path, VALUE rb_mode);
    VALUE test_closed();
    VALUE close();
    VALUE read(VALUE rb_size);
    VALUE write(VALUE rb_data);
    VALUE seek(VALUE rb_offset, VALUE rb_mode);
    VALUE tell();
    VALUE ftruncate(VALUE rb_size);
    
public:
    static void define_class();
    static VALUE rb_init(VALUE self, VALUE rb_afc, VALUE rb_path, VALUE rb_mode)
    {
        get_self(self)->init(rb_afc, rb_path, rb_mode);
        return self;
    }
    
    static void init(VALUE self, afc_client_t afc, uint64_t handle)
    {
        DBG("+ IO::AFC::FD: %p, %llx\n", afc, handle);
        IoAFCDescriptor* p = get_self(self);
        p->_afc = afc;
        p->_handle = handle;
    }
    
    static VALUE rb_test_closed(VALUE self)
    {
        return get_self(self)->test_closed();
    }

    static VALUE rb_close(VALUE self)
    {
        return get_self(self)->close();
    }
    
    static VALUE rb_read(VALUE self, VALUE rb_size)
    {
        return get_self(self)->read(rb_size);
    }
    
    static VALUE rb_write(VALUE self, VALUE rb_data)
    {
        return get_self(self)->write(rb_data);
    }
    
    static VALUE rb_seek(VALUE self, VALUE rb_offset, VALUE rb_mode)
    {
        return get_self(self)->seek(rb_offset, rb_mode);
    }
    
    static VALUE rb_tell(VALUE self)
    {
        return get_self(self)->tell();
    }
    
    static VALUE rb_ftruncate(VALUE self, VALUE rb_size)
    {
        return get_self(self)->ftruncate(rb_size);
    }
};



#pragma GCC visibility pop
#endif //__INCLUDE_IO_AFC_H__

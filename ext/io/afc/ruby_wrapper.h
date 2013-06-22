#include <ruby.h>

// C++/RUby wrapper
template<class T> class RubyWrapper
{
public:
    static void gc_mark(void* self) {};
    static VALUE rb_alloc(VALUE self)
    {
        T* p = (T*)ruby_xmalloc(sizeof(T));
        new((void*)p) T;
        return Data_Wrap_Struct(self, T::gc_mark, T::rb_free, p);
    }
    static void rb_free(void* obj)
    {
        ((T*)obj)->~T();
        ruby_xfree(obj);
    }
    static T* get_self(VALUE self)
    {
        T* t;
        Data_Get_Struct(self, T, t);
        return t;
    }
};

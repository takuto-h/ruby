
#include <limits.h>
#include <string.h>
#include <sys/inotify.h>
#include "ruby/ruby.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_SIZE_ENOUPH (EVENT_SIZE + NAME_MAX + 1)
#define EVENT_COUNT_ROUGH 8
#define BUFFER_SIZE (EVENT_SIZE_ENOUPH * EVENT_COUNT_ROUGH)

VALUE
saved_initialize(VALUE self)
{
    int inotify_fd;
    
    inotify_fd = inotify_init1(IN_CLOEXEC);
    if (inotify_fd < 0) {
        rb_sys_fail("inotify_init1");
    }
    rb_iv_set(self, "@inotify_fd", INT2NUM(inotify_fd));
    rb_iv_set(self, "@handlers", rb_hash_new());
    return self;
}

VALUE
saved_file(int argc, VALUE *argv, VALUE self)
{
    VALUE fname;
    VALUE proc;
    int inotify_fd;
    int watch_fd;
    VALUE handlers;
    
    rb_scan_args(argc, argv, "1&", &fname, &proc);
    inotify_fd = NUM2INT(rb_iv_get(self, "@inotify_fd"));
    watch_fd = inotify_add_watch(inotify_fd, StringValueCStr(fname), IN_CLOSE_WRITE);
    if (watch_fd < 0) {
        rb_sys_fail_str(fname);
    }
    handlers = rb_iv_get(self, "@handlers");
    rb_hash_aset(handlers, INT2NUM(watch_fd), proc);
    return self;
}

VALUE
saved_watch(VALUE self)
{
    int inotify_fd;
    VALUE handlers;
    size_t got_size;
    char buffer[BUFFER_SIZE];
    
    inotify_fd = NUM2INT(rb_iv_get(self, "@inotify_fd"));
    handlers = rb_iv_get(self, "@handlers");
    got_size = 0;
    for (;;) {
        ssize_t status;
        size_t treated_size;
        
        status = read(inotify_fd, &buffer[got_size], BUFFER_SIZE - got_size);
        if (status < 0) {
            rb_sys_fail("read");
        }
        got_size += status;
        treated_size = 0;
        while (treated_size < got_size) {
            struct inotify_event *event;
            VALUE proc;
            
            if (BUFFER_SIZE - treated_size < EVENT_SIZE_ENOUPH) {
                break;
            }
            event = (struct inotify_event *)&buffer[treated_size];
            proc = rb_hash_aref(handlers, INT2NUM(event->wd));
            rb_proc_call(proc, rb_ary_new());
            treated_size += EVENT_SIZE + event->len;
        }
        if (treated_size < got_size) {
            memmove(buffer, &buffer[treated_size], got_size - treated_size);
        }
        got_size -= treated_size;
    }
    return Qnil;
}

VALUE
saved_s_watch(VALUE klass)
{
    VALUE inst;
    
    inst = rb_funcall(klass, rb_intern("new"), 0);
    rb_funcall_passing_block(inst, rb_intern("instance_eval"), 0, NULL);
    return saved_watch(inst);
}

void
Init_saved(void)
{
    VALUE rb_cSaved;
    
    rb_cSaved = rb_define_class("Saved", rb_cObject);
    rb_define_method(rb_cSaved, "initialize", saved_initialize, 0);
    rb_define_method(rb_cSaved, "file", saved_file, -1);
    rb_define_method(rb_cSaved, "watch", saved_watch, 0);
    rb_define_singleton_method(rb_cSaved, "watch", saved_s_watch, 0);
}

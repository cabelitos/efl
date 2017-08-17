#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Ecore.h"
#include "efl_promise2.h"
#include "ecore_private.h"
#include <errno.h>
#include <stdarg.h>

#define EFL_FUTURE2_DISPATCHED ((Efl_Future2_Cb)(0x01))

#define EFL_MEMPOOL_CHECK_RETURN(_mp, _p)                               \
  if (!eina_mempool_from((_mp), (_p)))                                  \
    {                                                                   \
       ERR("The pointer %p does not belongs to mempool %p", (_p), (_mp)); \
       return;                                                          \
    }

#define EFL_MEMPOOL_CHECK_RETURN_VAL(_mp, _p, _val)                     \
  if (!eina_mempool_from((_mp), (_p)))                                  \
    {                                                                   \
       ERR("The pointer %p does not belongs to mempool %p", (_p), (_mp)); \
       return (_val);                                                   \
    }

#define EFL_PROMISE2_CHECK_RETURN(_p)                   \
  do {                                                  \
     EINA_SAFETY_ON_NULL_RETURN((_p));                  \
     EFL_MEMPOOL_CHECK_RETURN(_promise_mp, (_p));       \
  } while (0);

#define EFL_PROMISE2_CHECK_RETURN_VAL(_p, _val)                      \
  do {                                                               \
     EINA_SAFETY_ON_NULL_RETURN_VAL((_p), (_val));                   \
     EFL_MEMPOOL_CHECK_RETURN_VAL(_promise_mp, (_p), (_val));        \
  } while (0);

#define EFL_FUTURE2_CHECK_RETURN(_p)                    \
  do {                                                  \
     EINA_SAFETY_ON_NULL_RETURN((_p));                  \
     EFL_MEMPOOL_CHECK_RETURN(_future_mp, (_p));        \
     if (_p->cb == EFL_FUTURE2_DISPATCHED)              \
       {                                                \
          ERR("Future %p already dispatched", _p);      \
          return;                                       \
       }                                                \
  } while (0);

#define EFL_FUTURE2_CHECK_RETURN_VAL(_p, _val)                  \
  do {                                                          \
     EINA_SAFETY_ON_NULL_RETURN_VAL((_p), (_val));              \
     EFL_MEMPOOL_CHECK_RETURN_VAL(_future_mp, (_p), (_val));    \
     if (_p->cb == EFL_FUTURE2_DISPATCHED)                      \
       {                                                        \
          ERR("Future %p already dispatched", _p);              \
          return (_val);                                        \
       }                                                        \
  } while (0);

#undef ERR
#define ERR(...) EINA_LOG_DOM_ERR(_promise2_log_dom, __VA_ARGS__)

#undef DBG
#define DBG(...) EINA_LOG_DOM_DBG(_promise2_log_dom, __VA_ARGS__)

#undef INF
#define INF(...) EINA_LOG_DOM_INFO(_promise2_log_dom, __VA_ARGS__)

#undef WRN
#define WRN(...) EINA_LOG_DOM_WARN(_promise2_log_dom, __VA_ARGS__)

#undef CRI
#define CRI(...) EINA_LOG_DOM_CRIT(_promise2_log_dom, __VA_ARGS__)

#define _efl_promise2_value_dbg(_msg, _p, _v) __efl_promise2_value_dbg(_msg, _p, _v, __LINE__, __FUNCTION__)

struct _Efl_Promise2 {
   Efl_Future2 *future;
   Eina_Free_Cb cancel;
   const void *data;
};

struct _Efl_Future2 {
   Efl_Promise2 *promise;
   Efl_Future2 *next;
   Efl_Future2 *prev;
   Efl_Future2_Cb cb;
   const void *data;
   Eina_Value pending_value;
   Ecore_Event *event;
};

static Eina_Mempool *_promise_mp = NULL;
static Eina_Mempool *_future_mp = NULL;
static Eina_List *_pending_futures = NULL;
static Ecore_Event_Handler *_future_event_handler = NULL;
static int FUTURE_EVENT_ID = -1;
static int _promise2_log_dom = -1;

static void _efl_promise2_cancel(Efl_Promise2 *p);

static inline void
__efl_promise2_value_dbg(const char *msg,
                         const Efl_Promise2 *p,
                         const Eina_Value v,
                         int line,
                         const char *fname)
{
   if (EINA_UNLIKELY(eina_log_domain_level_check(_promise2_log_dom,
                                                 EINA_LOG_LEVEL_DBG)))
     {
        if (!v.type)
          {
             eina_log_print(_promise2_log_dom, EINA_LOG_LEVEL_DBG,
                            __FILE__, fname, line, "%s: %p with no value",
                            msg, p);
          }
        else
          {
             char *str = eina_value_to_string(&v);
             eina_log_print(_promise2_log_dom, EINA_LOG_LEVEL_DBG,
                            __FILE__, fname, line,
                            "%s: %p - Value Type: %s Content: %s", msg, p,
                            eina_value_type_name_get(eina_value_type_get(&v)),
                            str);
             free(str);
          }
     }
}

static Eina_Bool
_promise_setup(const Eina_Value_Type *type EINA_UNUSED, void *mem)
{
   Efl_Promise2 **tmem = mem;
   *tmem = NULL;
   return EINA_TRUE;
}

static Eina_Bool
_promise_flush(const Eina_Value_Type *type EINA_UNUSED, void *mem)
{
   Efl_Promise2 **tmem = mem;
   if (*tmem)
     {
        _efl_promise2_cancel(*tmem);
        *tmem = NULL;
     }
   return EINA_TRUE;
}

static void
_promise_replace(Efl_Promise2 **dst, Efl_Promise2 * const *src)
{
   if (*src == *dst) return;
   if (*dst) _efl_promise2_cancel(*dst);
   *dst = *src;
}

static Eina_Bool
_promise_vset(const Eina_Value_Type *type EINA_UNUSED, void *mem, va_list args)
{
   Efl_Promise2 **dst = mem;
   Efl_Promise2 **src = va_arg(args, Efl_Promise2 **);
   _promise_replace(dst, src);
   return EINA_TRUE;
}

static Eina_Bool
_promise_pset(const Eina_Value_Type *type EINA_UNUSED,
              void *mem, const void *ptr)
{
   Efl_Promise2 **dst = mem;
   Efl_Promise2 * const *src = ptr;
   _promise_replace(dst, src);
   return EINA_TRUE;
}

static Eina_Bool
_promise_pget(const Eina_Value_Type *type EINA_UNUSED,
              const void *mem, void *ptr)
{
   Efl_Promise2 * const *src = mem;
   Efl_Promise2 **dst = ptr;
   *dst = *src;
   return EINA_TRUE;
}

static Eina_Bool
_promise_convert_to(const Eina_Value_Type *type EINA_UNUSED, const Eina_Value_Type *convert, const void *type_mem, void *convert_mem)
{
   Efl_Promise2 * const *p = type_mem;

   if (convert == EINA_VALUE_TYPE_STRINGSHARE ||
       convert == EINA_VALUE_TYPE_STRING)
     {
        const char *other_mem;
        char buf[128];
        snprintf(buf, sizeof(buf), "Promise %p (cancel: %p, data: %p)",
                 *p, (*p)->cancel, (*p)->data);
        other_mem = buf;
        return eina_value_type_pset(convert, convert_mem, &other_mem);
     }
   return EINA_FALSE;
}

static const Eina_Value_Type EINA_VALUE_TYPE_PROMISE2 = {
  .version = EINA_VALUE_TYPE_VERSION,
  .value_size = sizeof(Efl_Promise2 *),
  .name = "Efl_Promise2",
  .setup = _promise_setup,
  .flush = _promise_flush,
  .copy = NULL,
  .compare = NULL,
  .convert_to = _promise_convert_to,
  .convert_from = NULL,
  .vset = _promise_vset,
  .pset = _promise_pset,
  .pget = _promise_pget
};

static Efl_Promise2 *
_eina_value_promise2_steal(Eina_Value *value)
{
   Efl_Promise2 **p, *r;
   /*
     Do not use eina_value_flush()/eina_value_pset() in here,
     otherwise it would cancel the promise!
   */
   p = eina_value_memory_get(value);
   r = *p;
   *p = NULL;
   return r;
}

static Efl_Future2 *
_efl_future2_free(Efl_Future2 *f)
{
   DBG("Free future %p", f);
   Efl_Future2 *next = f->next;
   if (next) next->prev = NULL;
   eina_mempool_free(_future_mp, f);
   return next;
}

static void
_efl_promise2_unlink(Efl_Promise2 *p)
{
   if (p->future)
     {
        DBG("Unliking promise %p and future %p", p, p->future);
        p->future->promise = NULL;
        p->future = NULL;
     }
}

static void
_efl_promise2_link(Efl_Promise2 *p, Efl_Future2 *f)
{
   if (p) p->future = f;
   f->promise = p;
   DBG("Linking future %p with promise %p", f, p);
}

static void
_efl_promise2_cancel(Efl_Promise2 *p)
{
   DBG("Cancelling promise: %p, data: %p, future: %p", p, p->data, p->future);
   _efl_promise2_unlink(p);
   p->cancel((void *)p->data);
   eina_mempool_free(_promise_mp, p);
}

static void
_eina_value_safe_flush(Eina_Value v)
{
   //FIXME: Should we move this check to eina_value?
   if (v.type) eina_value_flush(&v);
}

static void
_efl_promise2_value_steal_and_link(Eina_Value value, Efl_Future2 *f)
{
   Efl_Promise2 *p = _eina_value_promise2_steal(&value);
   DBG("Promise %p stolen from value", p);
   eina_value_flush(&value);
   _efl_promise2_link(p, f);
}

static Eina_Value
_efl_future2_cb_dispatch(Efl_Future2 *f, const Eina_Value value)
{
   Efl_Future2_Cb cb = f->cb;

   f->cb = EFL_FUTURE2_DISPATCHED;

   if (EINA_UNLIKELY(eina_log_domain_level_check(_promise2_log_dom, EINA_LOG_LEVEL_DBG)))
     {
        if (!value.type) DBG("Distach cb: %p, data: %p with no value", cb, f->data);
        else
          {
             char *str = eina_value_to_string(&value);
             DBG("Dispatch cb: %p, data: %p, value type: %s content: '%s'",
                 cb, f->data,
                 eina_value_type_name_get(eina_value_type_get(&value)), str);
             free(str);
          }
     }
   return cb((void *)f->data, value);
}

static Eina_Value
_efl_future2_dispatch_internal(Efl_Future2 **f,
                               const Eina_Value value)
{
   Eina_Value next_value = { 0 };

   assert(value.type != &EINA_VALUE_TYPE_PROMISE2);
   while ((*f) && (!(*f)->cb)) *f = _efl_future2_free(*f);
   if (!*f) return next_value;
   next_value = _efl_future2_cb_dispatch(*f, value);
   *f = _efl_future2_free(*f);
   return next_value;
}

static Eina_Bool
_eina_value_is(const Eina_Value v1, const Eina_Value v2)
{
   if (v1.type != v2.type) return EINA_FALSE;
   //Both types are NULL at this point... so they are equal...
   if (!v1.type) return EINA_TRUE;
   return !memcmp(eina_value_memory_get(&v1),
                  eina_value_memory_get(&v2),
                  v1.type->value_size);
}

static void
_efl_future2_dispatch(Efl_Future2 *f, Eina_Value value)
{
    Eina_Value next_value = _efl_future2_dispatch_internal(&f, value);
    if (!_eina_value_is(next_value, value)) _eina_value_safe_flush(value);
    if (!f)
      {
         if (next_value.type == &EINA_VALUE_TYPE_PROMISE2)
           {
              DBG("There are no more futures, but next_value is a promise setting p->future to NULL.");
              _efl_promise2_value_steal_and_link(next_value, NULL);
           }
         else _eina_value_safe_flush(next_value);
         return;
      }

    if (next_value.type == &EINA_VALUE_TYPE_PROMISE2)
      _efl_promise2_value_steal_and_link(next_value, f);
    else
      _efl_future2_dispatch(f, next_value);
 }

static Eina_Bool
_efl_future2_handler_cb(void *data EINA_UNUSED,
                        int type EINA_UNUSED,
                        void *event)
{
   Efl_Future2 *f = event;
   _pending_futures = eina_list_remove(_pending_futures, f);
   f->event = NULL;
   _efl_future2_dispatch(f, f->pending_value);
   return ECORE_CALLBACK_DONE;
}

static void
_dummy_free(void *user_data EINA_UNUSED, void *func_data EINA_UNUSED)
{
}

static void
_efl_future2_cancel(Efl_Future2 *f, int err)
{
   Eina_Value value;

   DBG("Cancelling future %p, cb: %p data: %p with error: %d - msg: '%s'",
       f, f->cb, f->data, err, eina_error_msg_get(err));

   for (; f->prev != NULL; f = f->prev)
     {
        assert(f->promise == NULL); /* intermediate futures shouldn't have a promise */
        assert(f->event == NULL); /* intermediate futures shouldn't have pending dispatch */
     }

   if (f->event)
     {
        ecore_event_del(f->event);
        f->event = NULL;
        _pending_futures = eina_list_remove(_pending_futures, f);
     }

   if (f->promise)
     {
        _efl_promise2_cancel(f->promise);
        f->promise = NULL;
     }

   eina_value_setup(&value, EINA_VALUE_TYPE_ERROR);
   eina_value_set(&value, err);

   while (f)
     {
        if (f->cb)
          {
             Eina_Value r = _efl_future2_cb_dispatch(f, value);
             if (!_eina_value_is(value, r)) _eina_value_safe_flush(r);
          }
        f = _efl_future2_free(f);
     }
   eina_value_flush(&value);
}

static void
_efl_future2_schedule(Efl_Promise2 *p,
                      Efl_Future2 *f,
                      Eina_Value value)
{
   f->event = ecore_event_add(FUTURE_EVENT_ID, f,
                              _dummy_free, NULL);
   EINA_SAFETY_ON_NULL_GOTO(f->event, err);
   f->pending_value = value;
   _pending_futures = eina_list_append(_pending_futures, f);
   DBG("The promise %p schedule the future %p with cb: %p and data: %p",
       p, f, f->cb, f->data);
   return;
 err:
   _efl_future2_cancel(p->future, ENOMEM);
}

static void
_efl_promise2_deliver(Efl_Promise2 *p,
                      Eina_Value value)
{
   if (p->future)
     {
        Efl_Future2 *f = p->future;
        _efl_promise2_unlink(p);
        if (value.type == &EINA_VALUE_TYPE_PROMISE2) _efl_promise2_value_steal_and_link(value, f);
        else _efl_future2_schedule(p, f, value);
     }
   else DBG("Promise %p has no future", p);
   eina_mempool_free(_promise_mp, p);
}

Eina_Bool
efl_promise2_init(void)
{
   const char *choice = getenv("EINA_MEMPOOL");
   if ((!choice) || (!choice[0])) choice = "chained_mempool";

   _promise2_log_dom = eina_log_domain_register("efl_promise2", EINA_COLOR_CYAN);
   if (_promise2_log_dom < 0)
     {
        EINA_LOG_ERR("Ecore was unable to create a log domain.");
        return EINA_FALSE;
     }

   //FIXME: Is 512 too high?
   _promise_mp = eina_mempool_add(choice, "Efl_Promise2",
                                  NULL, sizeof(Efl_Promise2), 512);
   EINA_SAFETY_ON_NULL_GOTO(_promise_mp, err_promise);

   _future_mp = eina_mempool_add(choice, "Efl_Future2",
                                 NULL, sizeof(Efl_Future2), 512);
   EINA_SAFETY_ON_NULL_GOTO(_future_mp, err_future);

   FUTURE_EVENT_ID = ecore_event_type_new();

   _future_event_handler = ecore_event_handler_add(FUTURE_EVENT_ID,
                                                   _efl_future2_handler_cb,
                                                   NULL);
   EINA_SAFETY_ON_NULL_GOTO(_future_event_handler, err_handler);

   return EINA_TRUE;

 err_handler:
   eina_mempool_del(_future_mp);
   _future_mp = NULL;
 err_future:
   eina_mempool_del(_promise_mp);
   _promise_mp = NULL;
 err_promise:
   eina_log_domain_unregister(_promise2_log_dom);
   _promise2_log_dom = -1;
   return EINA_FALSE;
}

void
efl_promise2_shutdown(void)
{
   while (_pending_futures) _efl_future2_cancel(_pending_futures->data, ECANCELED);
   ecore_event_handler_del(_future_event_handler);
   eina_mempool_del(_future_mp);
   eina_mempool_del(_promise_mp);
   eina_log_domain_unregister(_promise2_log_dom);
   _promise2_log_dom = -1;
   _promise_mp = NULL;
   _future_mp = NULL;
   _future_event_handler = NULL;
   FUTURE_EVENT_ID = -1;
}

EAPI Eina_Value
efl_promise2_as_value(Efl_Promise2 *p)
{
   Eina_Value v = { 0 };
   EFL_PROMISE2_CHECK_RETURN_VAL(p, v);
   eina_value_setup(&v, &EINA_VALUE_TYPE_PROMISE2);
   eina_value_pset(&v, &p);
   DBG("Created value from promise %p", p);
   return v;
}

static Eina_Value
_future_proxy(void *data, const Eina_Value v) {
   Eina_Value r;
   Efl_Promise2 *p = data;
   Efl_Future2 *f = p->future;

   _efl_promise2_value_dbg("Future proxy called. Resolving promise", p, v);
   _efl_promise2_unlink(p);
   //we're in a clean stack, no need to reschedule"
   r = _efl_future2_dispatch_internal(&f, v);
   if (!_eina_value_is(v, r)) _eina_value_safe_flush(r);
   eina_mempool_free(_promise_mp, p);
   return v;
}

static void
_proxy_cancel(void *data EINA_UNUSED)
{
}

EAPI Eina_Value
efl_future2_as_value(Efl_Future2 *f)
{
   Eina_Value v = { 0 };
   Efl_Promise2 *p;

   EFL_FUTURE2_CHECK_RETURN_VAL(f, v);
   p = efl_promise2_new(_proxy_cancel, NULL);
   EINA_SAFETY_ON_NULL_GOTO(p, err);
   efl_future2_then(f, _future_proxy, p);
   DBG("Creating future proxy for future: %p - promise %p", f, p);
   return efl_promise2_as_value(p);
 err:
   _efl_promise2_cancel(p);
   return v;
}

EAPI Efl_Promise2 *
efl_promise2_new(Eina_Free_Cb cancel_cb, const void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cancel_cb, NULL);
   Efl_Promise2 *p = eina_mempool_calloc(_promise_mp, sizeof(Efl_Promise2));
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, NULL);
   p->cancel = cancel_cb;
   p->data = data;
   DBG("Creating new promise - Promise:%p, cb: %p, data:%p", p,
       p->cancel, p->data);
   return p;
}

EAPI void
efl_future2_cancel(Efl_Future2 *f)
{
   EFL_FUTURE2_CHECK_RETURN(f);
   _efl_future2_cancel(f, ECANCELED);
}

EAPI void
efl_promise2_resolve(Efl_Promise2 *p, Eina_Value value)
{
   EFL_PROMISE2_CHECK_RETURN(p);
   _efl_promise2_value_dbg("Resolve promise", p, value);
   _efl_promise2_deliver(p, value);
}

EAPI void
efl_promise2_reject(Efl_Promise2 *p, Eina_Error err)
{
   EFL_PROMISE2_CHECK_RETURN(p);
   Eina_Value value;
   eina_value_setup(&value, EINA_VALUE_TYPE_ERROR);
   eina_value_set(&value, err);
   DBG("Reject promise %p - Error msg: '%s' - Error code: %d", p,
       eina_error_msg_get(err), err);
   _efl_promise2_deliver(p, value);
   return;
}

static Efl_Future2 *
_efl_future2_new(Efl_Promise2 *p, const Efl_Future2_Desc desc)
{
   Efl_Future2 *f;

   f = eina_mempool_calloc(_future_mp, sizeof(Efl_Future2));
   EINA_SAFETY_ON_NULL_RETURN_VAL(f, NULL);
   _efl_promise2_link(p, f);
   f->cb = desc.cb;
   f->data = desc.data;
   DBG("Creating new future - Promise:%p, Future:%p, cb: %p, data: %p ",
       p, f, f->cb, f->data);
   return f;
}

EAPI Efl_Future2 *
efl_future2_new(Efl_Promise2 *p)
{
   static const Efl_Future2_Desc desc = {
     .cb = NULL,
     .data = NULL
   };
   EFL_PROMISE2_CHECK_RETURN_VAL(p, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(p->future != NULL, NULL);
   return _efl_future2_new(p, desc);
}

static Efl_Future2 *
_efl_future2_then(Efl_Future2 *prev, const Efl_Future2_Desc desc)
{
   Efl_Future2 *next = _efl_future2_new(NULL, desc);
   EINA_SAFETY_ON_NULL_RETURN_VAL(next, NULL);
   next->prev = prev;
   prev->next = next;
   DBG("Linking futures - Prev:%p Next:%p", prev, next);
   return next;
}

EAPI Efl_Future2 *
efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc)
{
   EFL_FUTURE2_CHECK_RETURN_VAL(prev, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(prev->next != NULL, NULL);
   return _efl_future2_then(prev, desc);
}

EAPI Efl_Future2 *
efl_future2_chain_array(Efl_Future2 *prev, const Efl_Future2_Desc descs[])
{
   Efl_Future2 *f = prev;
   size_t i;

   EFL_FUTURE2_CHECK_RETURN_VAL(prev, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(prev->next != NULL, NULL);

   for (i = 0; descs[i].cb; i++)
     {
        f = _efl_future2_then(f, descs[i]);
        EINA_SAFETY_ON_NULL_GOTO(f, err);
     }

   return f;

 err:
   for (f = prev->next; f; f = f->next)
     eina_mempool_free(_future_mp, f);
   prev->next = NULL;
   return NULL;
}

typedef struct _Efl_Future2_Cb_Console_Ctx {
   char *prefix;
   char *suffix;
} Efl_Future2_Cb_Console_Ctx;

static Eina_Value
_efl_future2_cb_console(void *data,
                        const Eina_Value value)
{
   Efl_Future2_Cb_Console_Ctx *c = data;
   const char *prefix = c ? c->prefix : NULL;
   const char *suffix = c ? c->suffix : NULL;
   const char *content = "no value";
   char *str = NULL;

   if (value.type)
     {
        str = eina_value_to_string(&value);
        content = str;
     }

   if (!prefix) prefix = "";
   if (!suffix) suffix = "\n";
   printf("%s%s%s", prefix, content, suffix);
   free(str);
   if (c) {
      free(c->prefix);
      free(c->suffix);
      free(c);
   }
   return value;
}

EAPI Efl_Future2_Desc
efl_future2_cb_console(const char *prefix, const char *suffix)
{
   Efl_Future2_Cb_Console_Ctx *c;
   Efl_Future2_Desc desc = {
     .cb = _efl_future2_cb_console,
     .data = NULL
   };

   if (prefix || suffix) {
      desc.data = c = calloc(1, sizeof(Efl_Future2_Cb_Console_Ctx));
      EINA_SAFETY_ON_NULL_GOTO(c, exit);
      c->prefix = prefix ? strdup(prefix) : NULL;
      c->suffix = suffix ? strdup(suffix) : NULL;
   }
 exit:
   return desc;
}

static Eina_Value
_efl_future2_cb_convert_to(void *data, const Eina_Value src)
{
    const Eina_Value_Type *type = data;
    Eina_Value dst = { 0 };
    if (type && eina_value_setup(&dst, type)) eina_value_convert(&src, &dst);
    return dst;
}

EAPI Efl_Future2_Desc
efl_future2_cb_convert_to(const Eina_Value_Type *type)
{
   return (Efl_Future2_Desc){.cb = _efl_future2_cb_convert_to, .data = type};
}

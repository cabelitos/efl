#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Ecore.h"
#include "efl_promise2.h"
#include "ecore_private.h"
#include <errno.h>
#include <stdarg.h>

#define EFL_FUTURE2_DISPATCHED ((Efl_Future2_Cb)(0x01))
#define EFL_PROMISE2_CHECK_RETURN(_p)           \
  do {                                          \
     EINA_SAFETY_ON_NULL_RETURN((_p));          \
  } while (0);
#define EFL_PROMISE2_CHECK_RETURN_VAL(_p, _val)         \
  do {                                                  \
     EINA_SAFETY_ON_NULL_RETURN_VAL((_p), (_val));      \
  } while (0);
#define EFL_FUTURE2_CHECK_RETURN(_p)            \
  do {                                          \
     EINA_SAFETY_ON_NULL_RETURN((_p));          \
  } while (0);
#define EFL_FUTURE2_CHECK_RETURN_VAL(_p, _val)          \
  do {                                                  \
     EINA_SAFETY_ON_NULL_RETURN_VAL((_p), (_val));      \
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
   Eina_Value *pending_value;
   Ecore_Event *event;
};

typedef struct _Efl_Future2_Cb_Console_Ctx {
   char *prefix;
   char *suffix;
} Efl_Future2_Cb_Console_Ctx;

static Eina_Mempool *_promise_mp = NULL;
static Eina_Mempool *_future_mp = NULL;
static Eina_List *_pending_futures = NULL;
static Ecore_Event_Handler *_future_event_handler = NULL;
static int FUTURE_EVENT_ID = -1;
static int _promise2_log_dom = -1;

static void _efl_promise2_cancel(Efl_Promise2 *p);

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
   if (*dst)
     _efl_promise2_cancel(*dst);
   if (!*src)
     *dst = NULL;
   else
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

static const Eina_Value_Type EINA_VALUE_TYPE_PROMISE2 = {
  .version = EINA_VALUE_TYPE_VERSION,
  .value_size = sizeof(Efl_Promise2 *),
  .name = "Efl_Promise2",
  .setup = _promise_setup,
  .flush = _promise_flush,
  .copy = NULL,
  .compare = NULL,
  .convert_to = NULL,
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
   if (p)
     p->future = f;
   f->promise = p;
   DBG("Linking future %p with promise %p", f, p);
}

static void
_efl_promise2_cancel(Efl_Promise2 *p)
{
   DBG("Cancelling promise: %p", p);
   _efl_promise2_unlink(p);
   p->cancel((void *)p->data);
   eina_mempool_free(_promise_mp, p);
}

static void
_efl_promise2_value_steal(Eina_Value *value, Efl_Future2 *f)
{
   Efl_Promise2 *p = _eina_value_promise2_steal(value);
   DBG("Promise %p stolen from value %p", p, value);
   eina_value_free(value);
   _efl_promise2_link(p, f);
}

static Eina_Value *
_efl_future2_cb_dispatch(Efl_Future2 *f, Eina_Value *value)
{
   Efl_Future2_Cb cb;
   cb = f->cb;
   f->cb = EFL_FUTURE2_DISPATCHED;
   if (eina_log_domain_level_check(_promise2_log_dom, EINA_LOG_LEVEL_DBG))
     {
        char *str = eina_value_to_string(value);
        DBG("Dispatch %p + %p + %p with %s '%s'\n", cb, f->data, value,
            eina_value_type_name_get(eina_value_type_get(value)), str);
        free(str);
     }
   return cb((void *)f->data, value);
}

static void
_efl_future2_dispatch(Efl_Future2 *f, Eina_Value *value)
{
   Eina_Value *next_value;

   while (f)
     {
        if (f->cb)
          break;
        f = _efl_future2_free(f);
     }

   if (!f)
     {
        eina_value_free(value);
        return;
     }

   next_value = _efl_future2_cb_dispatch(f, value);
   if (value != next_value) eina_value_free(value);
   f = _efl_future2_free(f);

   if (!f)
     {
        eina_value_free(next_value);
        return;
     }

   if (next_value && next_value->type == &EINA_VALUE_TYPE_PROMISE2)
     _efl_promise2_value_steal(next_value, f);
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
   Eina_Value *value;

   if (f->cb == EFL_FUTURE2_DISPATCHED)
     return;

   DBG("Cancelling future %p, cb: %p data: %p with error: %d - msg: '%s'",
       f, f->cb, f->data, err, eina_error_msg_get(err));
   value = eina_value_new(EINA_VALUE_TYPE_ERROR);
   EINA_SAFETY_ON_NULL_RETURN(value);
   eina_value_set(value, err);

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
     _efl_promise2_cancel(f->promise);

   while (f)
     {
        if (f->cb)
          {
             Eina_Value *r;
             r = _efl_future2_cb_dispatch(f, value);
             eina_value_free(r);
          }
        f = _efl_future2_free(f);
     }
   eina_value_free(value);
}

static void
_efl_future2_schedule(Efl_Promise2 *p,
                      Eina_Value *value)
{
   if (p->future)
     {
        Efl_Future2 *f = p->future;
        _efl_promise2_unlink(p);
        if (value && value->type == &EINA_VALUE_TYPE_PROMISE2)
             _efl_promise2_value_steal(value, f);
        else
          {
             f->event = ecore_event_add(FUTURE_EVENT_ID, f,
                                        _dummy_free, NULL);
             EINA_SAFETY_ON_NULL_GOTO(f->event, err);
             f->pending_value = value;
             _pending_futures = eina_list_append(_pending_futures, f);
             DBG("The promise %p schedule the future %p with cb: %p and data: %p",
                 p, f, f->cb, f->data);
          }
     }
   else
     DBG("Promise %p has no future", p);
   eina_mempool_free(_promise_mp, p);
   return;
 err:
   _efl_future2_cancel(p->future, ENOMEM);
}

Eina_Bool
efl_promise2_init(void)
{
   const char *choice;

   choice = getenv("EINA_MEMPOOL");
   if ((!choice) || (!choice[0]))
     choice = "chained_mempool";

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
   Efl_Future2 *f;

   EINA_LIST_FREE(_pending_futures, f)
     efl_future2_cancel(f);

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

EAPI Eina_Value *
efl_promise2_as_value(Efl_Promise2 *p)
{
   EFL_PROMISE2_CHECK_RETURN_VAL(p, NULL);
   Eina_Value *v = eina_value_new(&EINA_VALUE_TYPE_PROMISE2);
   EINA_SAFETY_ON_NULL_RETURN_VAL(v, NULL);
   eina_value_pset(v, &p);
   DBG("Promise %p as value %p", p, v);
   return v;
}

static Eina_Value *
_future_proxy(void *data, const Eina_Value *v) {
   Efl_Promise2 *p = data;
   Eina_Value *new_v = NULL;

   if (v)
     {
        new_v = eina_value_new(v->type);
        EINA_SAFETY_ON_NULL_RETURN_VAL(new_v, NULL);
        if (!eina_value_copy(v, new_v))
          {
             ERR("Could not copy the Eina_Value type '%s'\n",
                 new_v->type->name);
             eina_value_setup(new_v, EINA_VALUE_TYPE_ERROR);
             eina_value_set(new_v, ENOTSUP);
          }
     }

   if (eina_log_domain_level_check(_promise2_log_dom, EINA_LOG_LEVEL_DBG))
     {
        char *str = eina_value_to_string(new_v);
        DBG("Future proxy called - scheduling promise: %p with - Value: %p, Type: %s, Content: %s",
            p, new_v,
            eina_value_type_name_get(eina_value_type_get(new_v)), str);
        free(str);
     }
   _efl_future2_schedule(p, new_v);
   return NULL;
}

static void
_dummy_cancel(void *data EINA_UNUSED)
{
}

EAPI Eina_Value *
efl_future2_as_value(Efl_Future2 *f)
{
   Efl_Promise2 *p;
   EFL_FUTURE2_CHECK_RETURN_VAL(f, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(f->cb == EFL_FUTURE2_DISPATCHED, NULL);

   p = efl_promise2_new(_dummy_cancel, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, NULL);
   efl_future2_then(f, _future_proxy, p);
   DBG("Creating future proxy for future: %p - promise %p", f, p);
   return efl_promise2_as_value(p);
}

EAPI Efl_Promise2 *
efl_promise2_new(Eina_Free_Cb cancel_cb, const void *data)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cancel_cb, NULL);
   Efl_Promise2 *p = eina_mempool_calloc(_promise_mp, sizeof(Efl_Promise2));
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, NULL);
   p->cancel = cancel_cb;
   p->data = data;
   DBG("Creting new promise - Promise:%p, cb: %p, data:%p", p,
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
efl_promise2_resolve(Efl_Promise2 *p, Eina_Value *value)
{
   EFL_PROMISE2_CHECK_RETURN(p);
   if (eina_log_domain_level_check(_promise2_log_dom, EINA_LOG_LEVEL_DBG))
     {
        char *str = eina_value_to_string(value);
        DBG("Resolve promise %p - Value: %p, Type: %s Content: %s", p, value,
            eina_value_type_name_get(eina_value_type_get(value)), str);
        free(str);
     }
   _efl_future2_schedule(p, value);
}

EAPI void
efl_promise2_reject(Efl_Promise2 *p, Eina_Error err)
{
   EFL_PROMISE2_CHECK_RETURN(p);
   Eina_Value *value = eina_value_new(EINA_VALUE_TYPE_ERROR);
   EINA_SAFETY_ON_NULL_GOTO(value, err);
   eina_value_set(value, err);
   DBG("Reject promise %p - Error msg: '%s' - Error code: %d", p,
       eina_error_msg_get(err), err);
   _efl_future2_schedule(p, value);
   return;
 err:
   if (p->future)
     _efl_future2_cancel(p->future, EINVAL);
   else
     eina_mempool_free(_promise_mp, p);
}

static Efl_Future2 *
_efl_future2_new(Efl_Promise2 *p, const Efl_Future2_Desc desc)
{
   Efl_Future2 *f;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(desc.api_version != EFL_FUTURE2_DESC_API_VERSION, NULL);
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
     .api_version = EFL_FUTURE2_DESC_API_VERSION,
     .cb = NULL,
     .data = NULL
   };
   if (p)
     {
        EFL_PROMISE2_CHECK_RETURN_VAL(p, NULL);
        EINA_SAFETY_ON_TRUE_RETURN_VAL(p->future != NULL, NULL);
     }
   return _efl_future2_new(p, desc);
}

static Efl_Future2 *
_efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc)
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
   return _efl_future2_then_from_desc(prev, desc);
}


EAPI Efl_Future2 *
efl_future2_chain_array(Efl_Future2 *prev, Efl_Future2_Desc descs[])
{
   Efl_Future2 *f = prev;
   size_t i;

   EFL_FUTURE2_CHECK_RETURN_VAL(prev, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(prev->next != NULL, NULL);

   for (i = 0; descs[i].cb; i++)
     {
        descs[i].api_version = EFL_FUTURE2_DESC_API_VERSION;
        f = _efl_future2_then_from_desc(f, descs[i]);
        EINA_SAFETY_ON_NULL_GOTO(f, err);
     }

   return f;

 err:
   for (f = prev->next; f; f = f->next)
     eina_mempool_free(_future_mp, f);
   prev->next = NULL;
   return NULL;
}

static Eina_Value *
_efl_future2_cb_console(void *data,
                        const Eina_Value *value)
{
   Efl_Future2_Cb_Console_Ctx *c = data;
   const char *prefix = c ? c->prefix : NULL;
   const char *suffix = c ? c->suffix : NULL;
   char *str = eina_value_to_string(value);

   if (!prefix) prefix = "";
   if (!suffix) suffix = "";
   printf("%s%s%s\n", prefix, str, suffix);
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
     .api_version = EFL_FUTURE2_DESC_API_VERSION,
     .cb = _efl_future2_cb_console,
     .data = NULL
   };

   if (prefix || suffix) {
      desc.data = c = calloc(1, sizeof(Efl_Future2_Cb_Console_Ctx));
      EINA_SAFETY_ON_NULL_GOTO(c, err_data);
      c->prefix = prefix ? strdup(prefix) : NULL;
      EINA_SAFETY_ON_NULL_GOTO(c->prefix, err_prefix);
      c->suffix = suffix ? strdup(suffix) : NULL;
      EINA_SAFETY_ON_NULL_GOTO(c->suffix, err_suffix);
   }
   return desc;
 err_suffix:
   free(c->prefix);
 err_prefix:
   free(c);
 err_data:
   desc.cb = NULL;
   return desc;
}

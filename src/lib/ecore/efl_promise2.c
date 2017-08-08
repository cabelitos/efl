#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "Ecore.h"
#include "efl_promise2.h"
#include "ecore_private.h"
#include <errno.h>

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

struct _Efl_Promise2 {
   Efl_Future2 *future;
   Eina_Free_Cb cancel;
   const void *data;
};

struct _Efl_Future2 {
   Efl_Promise2 *promise;
   Efl_Future2 *next;
   Efl_Future2 *prev;
   Efl_Future2_Cb success;
   Efl_Future2_Cb error;
   Eina_Free_Cb free;
   Eina_Value *pending_value;
   const void *data;
   Ecore_Event *event;
};

static Eina_Mempool *_promise_mp = NULL;
static Eina_Mempool *_future_mp = NULL;
static Eina_List *_pending_futures = NULL;
static int FUTURE_EVENT_ID = -1;
static Ecore_Event_Handler *_future_event_handler = NULL;

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
   *tmem = NULL;
   return EINA_TRUE;
}

static Eina_Bool
_promise_vset(const Eina_Value_Type *type EINA_UNUSED, void *mem, va_list args)
{
   Efl_Promise2 **dst = mem;
   Efl_Promise2 **src = va_arg(args, Efl_Promise2 **);
   *dst = *src;
   return EINA_TRUE;
}

static Eina_Bool
_promise_pset(const Eina_Value_Type *type EINA_UNUSED,
              void *mem, const void *ptr)
{
   Efl_Promise2 **dst = mem;
   Efl_Promise2 * const *src = ptr;
   *dst = *src;
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
   Efl_Promise2 *p, *aux = NULL;
   eina_value_pget(value, &p);
   eina_value_pset(value, &aux);
   return p;
}

static Efl_Future2 *
_efl_future2_free(Efl_Future2 *f)
{
   Efl_Future2 *next = f->next;
   if (next) next->prev = NULL;
   if (f->free) free((void *)f->data);
   eina_mempool_free(_future_mp, f);
   return next;
}

static void
_efl_promise2_unlink(Efl_Promise2 *p)
{
   if (p->future)
     {
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
}

static void
_efl_promise2_cancel(Efl_Promise2 *p)
{
   _efl_promise2_unlink(p);
   p->cancel((void *)p->data);
   eina_mempool_free(_promise_mp, p);
}

static void
_efl_promise2_steal(Eina_Value *value, Efl_Future2 *f)
{
   Efl_Promise2 *p = _eina_value_promise2_steal(value);
   eina_value_free(value);
   _efl_promise2_link(p, f);
}

static Eina_Value *
_efl_future2_cb_dispatch(Efl_Future2 *f, Eina_Value *value, Eina_Bool rejected)
{
   Efl_Future2_Cb cb;
   cb = rejected ? f->error : f->success;
   f->success = f->error = EFL_FUTURE2_DISPATCHED;
   return cb((void *)f->data, value);
}

static void
_efl_future2_dispatch(Efl_Future2 *f, Eina_Value *value)
{
   Eina_Value *next_value;
   Eina_Bool rejected = EINA_FALSE;

   if (value && value->type == EINA_VALUE_TYPE_ERROR)
     rejected = EINA_TRUE;

   while (f)
     {
        if (((!rejected && f->success) || (rejected && f->error)))
          break;
        f = _efl_future2_free(f);
     }

   if (!f)
     {
        eina_value_free(value);
        return;
     }

   next_value = _efl_future2_cb_dispatch(f, value, rejected);
   if (value != next_value) eina_value_free(value);
   f = _efl_future2_free(f);

   if (!f)
     {
        eina_value_free(next_value);
        return;
     }

   if (next_value && next_value->type == &EINA_VALUE_TYPE_PROMISE2)
     _efl_promise2_steal(next_value, f);
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

   if (f->success == EFL_FUTURE2_DISPATCHED)
     return;

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
        if (f->error)
          {
             Eina_Value *r;
             r = _efl_future2_cb_dispatch(f, value, EINA_TRUE);
             if (r && r->type == &EINA_VALUE_TYPE_PROMISE2)
               {
                  Efl_Promise2 *p = _eina_value_promise2_steal(r);
                  _efl_promise2_cancel(p);
               }
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
          _efl_promise2_steal(value, f);
        else
          {
             f->event = ecore_event_add(FUTURE_EVENT_ID, f,
                                        _dummy_free, NULL);
             EINA_SAFETY_ON_NULL_GOTO(f->event, err);
             f->pending_value = value;
             _pending_futures = eina_list_append(_pending_futures, f);
          }
     }
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

   //FIXME: Is 512 too high?
   _promise_mp = eina_mempool_add(choice, "Efl_Promise2",
                                  NULL, sizeof(Efl_Promise2), 512);
   EINA_SAFETY_ON_NULL_RETURN_VAL(_promise_mp, EINA_FALSE);

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
             printf("Could not copy the Eina_Value type '%s'\n",
                    new_v->type->name);
             eina_value_setup(new_v, EINA_VALUE_TYPE_ERROR);
             eina_value_set(new_v, ENOTSUP);
          }
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
   EINA_SAFETY_ON_TRUE_RETURN_VAL(f->success == EFL_FUTURE2_DISPATCHED, NULL);

   p = efl_promise2_new(_dummy_cancel, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, NULL);
   efl_future2_then(f, _future_proxy, _future_proxy, NULL, p);
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
   //One should use the efl_future2_reject() in this case...
   EINA_SAFETY_ON_TRUE_GOTO((value && value->type == EINA_VALUE_TYPE_ERROR), err);
   _efl_future2_schedule(p, value);
   return;
 err:
   if (p->future)
     _efl_future2_cancel(p->future, EINVAL);
   else
     eina_mempool_free(_promise_mp, p);
}

EAPI void
efl_promise2_reject(Efl_Promise2 *p, Eina_Error err)
{
   EFL_PROMISE2_CHECK_RETURN(p);
   Eina_Value *value = eina_value_new(EINA_VALUE_TYPE_ERROR);
   EINA_SAFETY_ON_NULL_GOTO(value, err);
   eina_value_set(value, err);
   _efl_future2_schedule(p, value);
   return;
 err:
   if (p->future)
     _efl_future2_cancel(p->future, EINVAL);
   else
     eina_mempool_free(_promise_mp, p);
}

static Efl_Future2 *
_efl_future2_new_from_desc(Efl_Promise2 *p, const Efl_Future2_Desc desc)
{
   Efl_Future2 *f;

   EINA_SAFETY_ON_TRUE_RETURN_VAL(desc.api_version != EFL_FUTURE2_DESC_API_VERSION,
                                  NULL);
   f = eina_mempool_calloc(_future_mp, sizeof(Efl_Future2));
   EINA_SAFETY_ON_NULL_RETURN_VAL(f, NULL);
   _efl_promise2_link(p, f);
   f->success = desc.success;
   f->error = desc.error;
   f->free = desc.free;
   f->data = desc.data;
   return f;
}

EAPI Efl_Future2 *
efl_future2_new(Efl_Promise2 *p)
{
   static const Efl_Future2_Desc desc = {
     EFL_FUTURE2_DESC_API_VERSION,
     NULL, NULL, NULL, NULL, NULL
   };
   EFL_PROMISE2_CHECK_RETURN_VAL(p, NULL);
   if (p)
     EINA_SAFETY_ON_TRUE_RETURN_VAL(p->future != NULL, NULL);
   return _efl_future2_new_from_desc(p, desc);
}

EAPI Efl_Future2 *
efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc)
{
   EFL_FUTURE2_CHECK_RETURN_VAL(prev, NULL);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(prev->next != NULL, NULL);

   Efl_Future2 *next = _efl_future2_new_from_desc(NULL, desc);
   EINA_SAFETY_ON_NULL_RETURN_VAL(next, NULL);
   next->prev = prev;
   prev->next = next;
   return next;
}

#include <Ecore.h>
#include <Eina.h>
#include <stdlib.h>
#include <errno.h>

typedef struct _Ctx {
   Efl_Promise2 *p;
   Eina_Bool should_fail;
   Eina_Bool with_msg;
   Ecore_Timer *timer;
} Ctx;

#define VALUE_TYPE_CHECK(_v, _type)                                     \
  if (_v.type != _type)                                                 \
    {                                                                   \
       fprintf(stderr, "Value type is not '%s' - received '%s'\n",      \
               _type->name, _v.type->name);                             \
       return _v;                                                       \
    }

static Eina_Bool
_timeout(void *data)
{
   Ctx *ctx = data;
   if (ctx->should_fail)
     efl_promise2_reject(ctx->p, ENETDOWN);
   else
     {
        Eina_Value v = { 0 };
        if (ctx->with_msg)
          {
            eina_value_setup(&v, EINA_VALUE_TYPE_STRING);
            eina_value_set(&v, "the simple example is working!");
          }
        efl_promise2_resolve(ctx->p, v);
     }
   free(ctx);
   return EINA_FALSE;
}

static void
_promise_cancel(void *data)
{
   Ctx *ctx = data;
   if (ctx->timer)
     ecore_timer_del(ctx->timer);
   free(ctx);
}

static Efl_Future2 *
_simple_future_get(Eina_Bool should_fail, Eina_Bool with_msg)
{
   Ctx *ctx = malloc(sizeof(Ctx));
   EINA_SAFETY_ON_NULL_RETURN_VAL(ctx, NULL);
   ctx->p = efl_promise2_new(_promise_cancel, ctx);
   EINA_SAFETY_ON_NULL_GOTO(ctx->p, err_promise);
   ctx->should_fail = should_fail;
   ctx->with_msg = with_msg;
   ctx->timer = ecore_timer_add(0.1, _timeout, ctx);
   return efl_future2_new(ctx->p);

 err_promise:
   free(ctx);
   return NULL;
}

static Eina_Value
_simple_ok(void *data EINA_UNUSED, const Eina_Value v)
{
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   return v;
}


static Eina_Value
_alternate_error_cb(void *data, const Eina_Value v)
{
   Eina_Bool *should_fail = data;
   Eina_Value new_v = { 0 };

   if (*should_fail)
     {
        *should_fail = EINA_FALSE;
        eina_value_setup(&new_v, EINA_VALUE_TYPE_ERROR);
        eina_value_set(&new_v, ENETDOWN);
        printf("Received succes from the previous future - Generating error for the next future...\n");
     }
   else
     {
        *should_fail = EINA_TRUE;
        Eina_Error err;
        VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_ERROR);
        eina_value_get(&v, &err);
        printf("Received error from the previous future - value: %s. Send success\n",
               eina_error_msg_get(err));
     }
   return new_v;
}

static void
_alternate_error(void)
{
   static Eina_Bool should_fail = EINA_TRUE;

   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                     {.cb = _alternate_error_cb, .data = &should_fail},
                     {.cb = _alternate_error_cb, .data = &should_fail},
                     {.cb = _alternate_error_cb, .data = &should_fail},
                     {.cb = _alternate_error_cb, .data = &should_fail});

}

static Eina_Value
_simple_err(void *data EINA_UNUSED, const Eina_Value v)
{
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_ERROR);
   return v;
}

static void
_simple(void)
{
   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_TRUE),
                     efl_future2_cb_console(NULL, NULL),
                     { .cb = _simple_ok, .data = NULL });
   efl_future2_chain(_simple_future_get(EINA_TRUE, EINA_FALSE),
                     efl_future2_cb_console(NULL, NULL),
                     { .cb = _simple_err, .data = NULL });
}

static Eina_Value
_chain_no_errors_cb(void *data EINA_UNUSED, const Eina_Value v)
{
   int count;
   Eina_Value new_v;

   eina_value_setup(&new_v, EINA_VALUE_TYPE_INT);
   if (!v.type)
     count = 1;
   else
     {
        VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_INT);
        eina_value_get(&v, &count);
     }
   eina_value_set(&new_v, count * 2);
   return new_v;
}

static void
_chain_no_errors(void)
{
   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                     efl_future2_cb_console(NULL, NULL),
                     {.cb = _chain_no_errors_cb, .data = NULL},
                     efl_future2_cb_console(NULL, NULL),
                     {.cb = _chain_no_errors_cb, .data = NULL},
                     efl_future2_cb_console(NULL, NULL),
                     {.cb = _chain_no_errors_cb, .data = NULL},
                     efl_future2_cb_console(NULL, NULL),
                     {.cb = _chain_no_errors_cb, .data = NULL},
                     efl_future2_cb_console(NULL, NULL));
}

static Eina_Value
_chain_with_error_cb(void *data EINA_UNUSED, const Eina_Value v EINA_UNUSED)
{
   Eina_Value err;
   eina_value_setup(&err, EINA_VALUE_TYPE_ERROR);
   eina_value_set(&err, E2BIG);
   return err;
}

static void
_chain_with_error(void)
{
   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                     { _chain_with_error_cb, NULL },
                     efl_future2_cb_console(NULL, NULL),
                     { .cb = _simple_err, .data = NULL });
}

static Eina_Value
_delayed_resolve(void *data, const Eina_Value v)
{
   Efl_Promise2 *p = data;
   Eina_Value new_v;
   eina_value_setup(&new_v, EINA_VALUE_TYPE_STRING);
   eina_value_set(&new_v, "Hello from inner future");
   efl_promise2_resolve(p, new_v);
   return v;
}

static Eina_Value
_delayed_reject(void *data, const Eina_Value v)
{
   Efl_Promise2 *p = data;
   efl_promise2_reject(p, ENETDOWN);
   return v;
}

static void
_dummy(void *data EINA_UNUSED)
{
}

static Eina_Value
_chain_inner_cb(void *data, const Eina_Value v EINA_UNUSED)
{
   Eina_Value r = { 0 };
   Efl_Promise2 *p = efl_promise2_new(_dummy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, r);
   printf("Creating a new promise inside the future cb\n");
   efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                    !data ? _delayed_resolve : _delayed_reject,
                    p);
   return efl_promise2_as_value(p);
}

static Eina_Value
_chain_inner_last_cb(void *data EINA_UNUSED, const Eina_Value v)
{
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   return v;
}

static void
_chain_inner_no_errors(void)
{
   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                     { .cb = _chain_inner_cb, .data = NULL },
                     efl_future2_cb_console(NULL, NULL),
                     { .cb = _chain_inner_last_cb, .data = NULL });
}

static Eina_Value
_err_inner_chain(void *data EINA_UNUSED, const Eina_Value v)
{
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_ERROR);
   ecore_main_loop_quit();
   return v;
}

static void
_chain_inner_errors(void)
{

   efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                     { .cb = _chain_inner_cb, .data = (void *)1 },
                     efl_future2_cb_console(NULL, NULL),
                     { .cb = _err_inner_chain, .data = NULL });
}

static Eina_Value
_canceled_cb(void *data EINA_UNUSED, const Eina_Value v)
{
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_ERROR);
   return v;
}

static void
_future_cancel(void)
{
   Efl_Future2 *f;

   f = efl_future2_chain(_simple_future_get(EINA_FALSE, EINA_FALSE),
                         efl_future2_cb_console(NULL, NULL),
                         { .cb = _canceled_cb, .data = NULL },
                         efl_future2_cb_console(NULL, NULL),
                         { .cb = _canceled_cb, .data = NULL },
                         efl_future2_cb_console(NULL, NULL),
                         { .cb = _canceled_cb, .data = NULL },
                         efl_future2_cb_console(NULL, NULL),
                         { .cb = _canceled_cb, .data = NULL },
                         efl_future2_cb_console(NULL, NULL),
                         { .cb = _canceled_cb, .data = NULL },
                         efl_future2_cb_console(NULL, NULL));
   efl_future2_cancel(f);
}

int
main(int argc EINA_UNUSED, char *argv[] EINA_UNUSED)
{
   if (!eina_init())
     {
        fprintf(stderr, "Could not init eina\n");
        return EXIT_FAILURE;
     }

   if (!ecore_init())
     {
        fprintf(stderr, "Could not init ecore\n");
        goto err_ecore;
     }

   _simple();
   _alternate_error();
   _chain_no_errors();
   _chain_with_error();
   _chain_inner_no_errors();
   _chain_inner_errors();
   _future_cancel();

   ecore_main_loop_begin();

   eina_shutdown();
   ecore_shutdown();
   return EXIT_SUCCESS;

 err_ecore:
   eina_shutdown();
   return EXIT_FAILURE;
}

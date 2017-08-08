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

static Eina_Bool
_timeout(void *data)
{
   Ctx *ctx = data;
   if (ctx->should_fail)
     efl_promise2_reject(ctx->p, ENETDOWN);
   else
     {
        if (ctx->with_msg)
          {
             Eina_Value *v = eina_value_new(EINA_VALUE_TYPE_STRING);
             if (v)
               {
                  eina_value_set(v, "the simple example is working!");
                  efl_promise2_resolve(ctx->p, v);
               }
          }
        else
          efl_promise2_resolve(ctx->p, NULL);
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

static Eina_Value *
_never_execute(void *data EINA_UNUSED, const Eina_Value *v EINA_UNUSED)
{
   printf("THIS FUNCTION SHOULD NEVER BE EXECUTED!\n");
   return NULL;
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

static Eina_Value *
_simple_ok(void *data EINA_UNUSED, const Eina_Value *v)
{
   const char *msg;
   eina_value_get(v, &msg);
   printf("Simple future completed - msg: %s\n", msg);
   return NULL;
}


static Eina_Value *
_alternate_error_cb(void *data, const Eina_Value *v)
{
   Eina_Bool *should_fail = data;
   Eina_Value *new_v = NULL;

   if (*should_fail)
     {
        *should_fail = EINA_FALSE;
        new_v = eina_value_new(EINA_VALUE_TYPE_ERROR);
        EINA_SAFETY_ON_NULL_RETURN_VAL(new_v, NULL);
        eina_value_set(new_v, ENETDOWN);
        printf("Received succes from the previous future - Generating error for the next future...\n");
     }
   else
     {
        *should_fail = EINA_TRUE;
        Eina_Error err;
        eina_value_get(v, &err);
        printf("Received error from the previous future - value: %s. Send success\n",
               eina_error_msg_get(err));
     }
   return new_v;
}

static void
_alternate_error(void)
{
   Efl_Future2 *f;
   static Eina_Bool should_fail = EINA_TRUE;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _alternate_error_cb,
                        _never_execute, NULL, &should_fail);
   f = efl_future2_then(f, _never_execute,
                        _alternate_error_cb, NULL, &should_fail);
   f = efl_future2_then(f,
                        _alternate_error_cb,
                        _never_execute, NULL, &should_fail);
   efl_future2_then(f, _never_execute,
                    _alternate_error_cb,  NULL, &should_fail);

}

static Eina_Value *
_simple_err(void *data EINA_UNUSED, const Eina_Value *v)
{
   Eina_Error err;
   eina_value_get(v, &err);
   fprintf(stderr, "Simple future reject msg: %s\n", eina_error_msg_get(err));
   return NULL;
}

static void
_simple(void)
{
   efl_future2_then(_simple_future_get(EINA_FALSE, EINA_TRUE), _simple_ok,
                    _never_execute, NULL, NULL);
   efl_future2_then(_simple_future_get(EINA_TRUE, EINA_FALSE), _never_execute,
                    _simple_err, NULL, NULL);
}

static Eina_Value *
_chain_no_errors_cb(void *data EINA_UNUSED, const Eina_Value *v)
{
   int count;
   Eina_Value *new_v = eina_value_new(EINA_VALUE_TYPE_INT);
   EINA_SAFETY_ON_NULL_RETURN_VAL(new_v, NULL);

   if (!v)
     count = 1;
   else
     eina_value_get(v, &count);

   printf("Future chain resolved with value: %d\n", count);
   eina_value_set(new_v, count * 2);
   return new_v;
}

static void
_chain_no_errors(void)
{
   Efl_Future2 *f;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _chain_no_errors_cb,
                        _never_execute, NULL, NULL);
   f = efl_future2_then(f, _chain_no_errors_cb,
                        _never_execute, NULL, NULL);
   f = efl_future2_then(f, _chain_no_errors_cb,
                        _never_execute, NULL, NULL);
   efl_future2_then(f, _chain_no_errors_cb, _never_execute, NULL, NULL);
}

static Eina_Value *
_chain_with_error_cb(void *data EINA_UNUSED, const Eina_Value *v EINA_UNUSED)
{
   Eina_Value *err = eina_value_new(EINA_VALUE_TYPE_ERROR);
   EINA_SAFETY_ON_NULL_RETURN_VAL(err, NULL);
   printf("Returning error to the future chain...\n");
   eina_value_set(err, E2BIG);
   return err;
}

static void
_chain_with_error(void)
{
   Efl_Future2 *f;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _chain_with_error_cb,
                        _never_execute, NULL, NULL);
   f = efl_future2_then(f, _never_execute,
                        NULL, NULL, NULL);
   efl_future2_then(f, _never_execute,
                    _simple_err, NULL, NULL);
}

static Eina_Value *
_delayed_resolve(void *data, const Eina_Value *v EINA_UNUSED)
{
   Efl_Promise2 *p = data;
   Eina_Value *new_v = eina_value_new(EINA_VALUE_TYPE_STRING);
   EINA_SAFETY_ON_NULL_RETURN_VAL(new_v, NULL);
   eina_value_set(new_v, "Hello from inner future");
   printf("Inner future resolved first\n");
   efl_promise2_resolve(p, new_v);
   return NULL;
}

static Eina_Value *
_delayed_reject(void *data, const Eina_Value *v EINA_UNUSED)
{
   Efl_Promise2 *p = data;
   printf("Inner future is now failing...\n");
   efl_promise2_reject(p, ENETDOWN);
   return NULL;
}

static void
_dummy(void *data EINA_UNUSED)
{
}

static Eina_Value *
_chain_inner_cb(void *data, const Eina_Value *v EINA_UNUSED)
{
   Efl_Promise2 *p = efl_promise2_new(_dummy, NULL);
   EINA_SAFETY_ON_NULL_RETURN_VAL(p, NULL);
   printf("Creating a new promise inside the future cb\n");
   efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE), !data ? _delayed_resolve : _delayed_reject,
                    _never_execute, NULL, p);
   return efl_promise2_as_value(p);
}

static Eina_Value *
_chain_inner_last_cb(void *data EINA_UNUSED, const Eina_Value *v)
{
   const char *msg = NULL;
   eina_value_get(v, &msg);
   printf("Outer future is now resolved with result: %s\n", msg);
   return NULL;
}

static void
_chain_inner_no_errors(void)
{
   Efl_Future2 *f;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _chain_inner_cb,
                        _never_execute, NULL, NULL);
   efl_future2_then(f, _chain_inner_last_cb, _never_execute, NULL, NULL);
}

static Eina_Value *
_err_inner_chain(void *data EINA_UNUSED, const Eina_Value *v)
{
   Eina_Error err;
   eina_value_get(v, &err);
   fprintf(stderr, "Inner future failed with msg: %s\n", eina_error_msg_get(err));
   ecore_main_loop_quit();
   return NULL;
}

static void
_chain_inner_errors(void)
{
   Efl_Future2 *f;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _chain_inner_cb,
                        _never_execute, NULL, (void *)1);
   efl_future2_then(f, _never_execute, _err_inner_chain, NULL, NULL);
}

static Eina_Value *
_canceled_cb(void *data EINA_UNUSED, const Eina_Value *v)
{
   Eina_Error err;
   eina_value_get(v, &err);
   printf("Future canceled - %s\n", eina_error_msg_get(err));
   return NULL;
}

static void
_future_cancel(void)
{
   Efl_Future2 *f;

   f = efl_future2_then(_simple_future_get(EINA_FALSE, EINA_FALSE),
                        _never_execute,
                        _canceled_cb, NULL, NULL);
   f = efl_future2_then(f, _never_execute,
                        _canceled_cb, NULL, NULL);
   f = efl_future2_then(f, _never_execute,
                        _canceled_cb, NULL, NULL);
   f = efl_future2_then(f, _never_execute,
                        _canceled_cb, NULL, NULL);
   f = efl_future2_then(f, _never_execute,
                        _canceled_cb, NULL, NULL);
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

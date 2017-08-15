#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include <Eina.h>
#include <stdlib.h>
#include <errno.h>
#include "ecore_suite.h"

#define CHAIN_SIZE (3)
#define DEFAULT_ERROR (EFBIG)
#define DEFAULT_MSG ("Future resolve is working!")

#define VALUE_TYPE_CHECK(_v, _type)                     \
  do {                                                  \
     fail_if(!(_v));                                    \
     fail_if((_v)->type != (_type));                    \
  } while(0)

#define ERROR_CHECK(_v, _errno)                                 \
  do {                                                          \
     Eina_Error _err;                                           \
     VALUE_TYPE_CHECK(_v, EINA_VALUE_TYPE_ERROR);               \
     fail_if(!eina_value_get(_v, &_err));                       \
     fail_if(_err != _errno);                                   \
  } while (0)

typedef struct _PromiseCtx {
   Efl_Promise2 *p;
   Ecore_Timer *t;
   Eina_Bool fail;
   Eina_Bool use_int;
} PromiseCtx;

static void
_cancel(void *data)
{
   PromiseCtx *ctx = data;
   if (ctx->t)
     ecore_timer_del(ctx->t);
   ctx->t = NULL;
   free(ctx);
}

static void
_promise_cancel_test(void *data)
{
   Eina_Bool *cancel_called = data;
   *cancel_called = EINA_TRUE;
}

static void
_dummy_cancel(void *data EINA_UNUSED)
{
}

static Eina_Bool
_simple_timeout(void *data)
{
   PromiseCtx *ctx = data;

   if (ctx->fail)
     efl_promise2_reject(ctx->p, DEFAULT_ERROR);
   else
     {
        Eina_Value *v = eina_value_new(ctx->use_int ?
                                       EINA_VALUE_TYPE_INT : EINA_VALUE_TYPE_STRING);
        fail_if(!v);
        fail_if(!eina_value_set(v, ctx->use_int ?  0 : DEFAULT_MSG));
        efl_promise2_resolve(ctx->p, v);
     }
   free(ctx);
   return EINA_FALSE;
}

static Efl_Future2 *
_future_get(Eina_Bool fail, Eina_Bool use_int)
{
   Efl_Promise2 *p;
   PromiseCtx *ctx;
   Efl_Future2 *f;

   ctx = calloc(1, sizeof(PromiseCtx));
   fail_if(!ctx);
   ctx->fail = fail;
   p = efl_promise2_new(_cancel, ctx);
   fail_if(!p);
   ctx->p = p;
   f = efl_future2_new(p);
   fail_if(!f);
   ctx->t = ecore_timer_add(0.1, _simple_timeout, ctx);
   fail_if(!ctx->t);
   ctx->use_int = use_int;
   return f;
}

static Eina_Value *
_simple_err(void *data EINA_UNUSED, const Eina_Value *v)
{
   ERROR_CHECK(v, DEFAULT_ERROR);
   ecore_main_loop_quit();
   return NULL;
}

static Eina_Value *
_simple_ok(void *data EINA_UNUSED, const Eina_Value *v)
{
   const char *msg;

   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   fail_if(!eina_value_get(v, &msg));
   fail_if(strcmp(DEFAULT_MSG, msg));
   ecore_main_loop_quit();
   return NULL;
}

static Eina_Value *
_chain_stop(void *data, const Eina_Value *v EINA_UNUSED)
{
   int *i = data;
   fail_if(*i != CHAIN_SIZE);
   ecore_main_loop_quit();
   return NULL;
}

static Eina_Value *
_chain_no_error(void *data, const Eina_Value *v)
{
   Eina_Value *new_v;
   static int count = 0;
   int current_i;
   int *i = data;

   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_INT);
   fail_if(!eina_value_get(v, &current_i));
   fail_if(current_i != count++);
   new_v = eina_value_new(EINA_VALUE_TYPE_INT);
   fail_if(!new_v);
   fail_if(!eina_value_set(new_v, count));
   (*i)++;
   return new_v;
}

static Eina_Value *
_chain_error(void *data, const Eina_Value *v)
{
   int *i = data;

   ERROR_CHECK(v, DEFAULT_ERROR);
   (*i)++;
   return v;
}

static void
_simple_test(Eina_Bool fail)
{
   Efl_Future2 *f;

   fail_if(!ecore_init());
   f = efl_future2_then(_future_get(fail, EINA_FALSE),
                        fail ? _simple_err : _simple_ok, NULL);
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}

static void
_simple_chain_test(Eina_Bool fail)
{
   Efl_Future2 *f;
   static int i = 0;
   Efl_Future2_Cb cb = fail ? _chain_error : _chain_no_error;

   fail_if(!ecore_init());
   f = efl_future2_chain(_future_get(fail, fail ? EINA_FALSE : EINA_TRUE),
                         {.cb = cb, .data = &i},
                         {.cb = cb, .data = &i},
                         {.cb = cb, .data = &i},
                         {.cb = _chain_stop, .data = &i});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}

static Eina_Value *
_cancel_cb(void *data, const Eina_Value *v)
{
   Eina_Value *new_v;
   int *cancel_count = data;

   new_v = eina_value_new(EINA_VALUE_TYPE_INT);
   fail_if(!new_v);
   ERROR_CHECK(v, ECANCELED);
   (*cancel_count)++;
   /* Although this function returns an INT Eina_Value, the next
      _cancel_cb must receive a EINA_VALYE_TYPE_ERROR as ECANCELED */
   return new_v;
}

static Eina_Value *
_inner_resolve(void *data EINA_UNUSED, const Eina_Value *v EINA_UNUSED)
{
   Eina_Value *new_v = eina_value_new(EINA_VALUE_TYPE_STRING);
   fail_if(!new_v);
   fail_if(!eina_value_set(new_v, DEFAULT_MSG));
   efl_promise2_resolve(data, new_v);
   return NULL;
}

static Eina_Value *
_inner_fail(void *data EINA_UNUSED, const Eina_Value *v EINA_UNUSED)
{
   efl_promise2_reject(data, DEFAULT_ERROR);
   return NULL;
}

static Eina_Value *
_future_promise_create(void *data, const Eina_Value *v EINA_UNUSED)
{
   Eina_Value *new_v;
   Efl_Promise2 *p;

   p = efl_promise2_new(_dummy_cancel, NULL);
   fail_if(!p);
   new_v = efl_promise2_as_value(p);
   fail_if(!v);
   efl_future2_then(_future_get(EINA_FALSE, EINA_FALSE),
                    data ? _inner_fail : _inner_resolve,
                    p);
   return new_v;
}

static Eina_Value *
_inner_future_last(void *data, const Eina_Value *v)
{
   if (data)
     ERROR_CHECK(v, DEFAULT_ERROR);
   else
     {
        const char *msg;
        VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
        fail_if(!eina_value_get(v, &msg));
        fail_if(strcmp(DEFAULT_MSG, msg));
     }
   ecore_main_loop_quit();
   return NULL;
}

static void
_inner_promise_test(Eina_Bool fail)
{
   Efl_Future2 *f;
   void *data = fail ? (void *) 0x01 : NULL;

   fail_if(!ecore_init());
   f = efl_future2_chain(_future_get(EINA_FALSE, EINA_FALSE),
                         {.cb = _future_promise_create, .data = data},
                         {.cb = _inner_future_last, .data = data});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();

}

START_TEST(efl_test_promise_future_success)
{
   _simple_test(EINA_FALSE);
}
END_TEST

START_TEST(efl_test_promise_future_failure)
{
   _simple_test(EINA_TRUE);
}
END_TEST

START_TEST(efl_test_promise_future_chain_no_error)
{
   _simple_chain_test(EINA_FALSE);
}
END_TEST

START_TEST(efl_test_promise_future_chain_error)
{
   _simple_chain_test(EINA_TRUE);
}
END_TEST

START_TEST(efl_test_promise_future_cancel)
{
   fail_if(!ecore_init());
   int i;

   for (i = 0; i < 3; i++)
     {
        Efl_Promise2 *p;
        Efl_Future2 *first, *last, *middle;
        int cancel_count = 0;
        Eina_Bool cancel_called = EINA_FALSE;

        p = efl_promise2_new(_promise_cancel_test, &cancel_called);
        fail_if(!p);
        first = efl_future2_new(p);
        fail_if(!first);
        if (i == 2)
          {
             Efl_Future2 *f;
             last = NULL;
             f = efl_future2_then(first, _cancel_cb, &cancel_count);
             fail_if(!f);
             middle = efl_future2_then(f, _cancel_cb, &cancel_count);
             fail_if(!middle);
             f = efl_future2_then(middle, _cancel_cb, &cancel_count);
             fail_if(!f);
          }
        else
          {
             middle = NULL;
             last = efl_future2_chain(first,
                                      {.cb = _cancel_cb, .data = &cancel_count},
                                      {.cb = _cancel_cb, .data = &cancel_count},
                                      {.cb = _cancel_cb, .data = &cancel_count});
             fail_if(!last);
          }
        if (i == 0)
          efl_future2_cancel(last);
        else if (i == 1)
          efl_future2_cancel(first);
        else
          efl_future2_cancel(middle);
        fail_if(cancel_count != CHAIN_SIZE);
        fail_if(!cancel_called);
     }
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_inner_promise)
{
   _inner_promise_test(EINA_FALSE);
}
END_TEST


START_TEST(efl_test_promise_future_inner_promise_fail)
{
   _inner_promise_test(EINA_TRUE);
}
END_TEST

START_TEST(efl_test_promise_future_implicit_cancel)
{
   Efl_Promise2 *p;
   Efl_Future2 *f;
   int cancel_count = 0;

   fail_if(!ecore_init());

   p = efl_promise2_new(_dummy_cancel, NULL);
   fail_if(!p);
   f = efl_future2_new(p);
   fail_if(!f);
   f = efl_future2_chain(f,
                         {.cb = _cancel_cb, .data = &cancel_count},
                         {.cb = _cancel_cb, .data = &cancel_count},
                         {.cb = _cancel_cb, .data = &cancel_count});
   fail_if(!f);
   efl_promise2_resolve(p, NULL);
   ecore_shutdown();
   //It should be cancelled by ecore_shutdown()
   fail_if(cancel_count != CHAIN_SIZE);
}
END_TEST

void ecore_test_ecore_promise2(TCase *tc)
{
   tcase_add_test(tc, efl_test_promise_future_success);
   tcase_add_test(tc, efl_test_promise_future_failure);
   tcase_add_test(tc, efl_test_promise_future_chain_no_error);
   tcase_add_test(tc, efl_test_promise_future_chain_error);
   tcase_add_test(tc, efl_test_promise_future_cancel);
   tcase_add_test(tc, efl_test_promise_future_implicit_cancel);
   tcase_add_test(tc, efl_test_promise_future_inner_promise);
   tcase_add_test(tc, efl_test_promise_future_inner_promise_fail);
}

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <Ecore.h>
#include <Eina.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "ecore_suite.h"

#define CHAIN_SIZE (3)
#define DEFAULT_ERROR (EFBIG)
#define DEFAULT_MSG ("Future resolve is working!")
#define DEFAULT_INT_VALUE (5466)
#define DEFAULT_INT_VALUE_AS_STRING ("5466")
#define DEFAULT_TIMEOUT (0.1)

#define VALUE_TYPE_CHECK(_v, _type)                     \
  do {                                                  \
     ck_assert_ptr_eq(_v.type, _type);                  \
} while(0)

#define ERROR_CHECK(_v, _errno)                                 \
  do {                                                          \
     Eina_Error _err;                                           \
     VALUE_TYPE_CHECK(_v, EINA_VALUE_TYPE_ERROR);               \
     fail_if(!eina_value_get(&_v, &_err));                      \
     ck_assert_int_eq(_err, _errno);                            \
  } while (0)

typedef struct _PromiseCtx {
   Efl_Promise2 *p;
   Ecore_Timer *t;
   Eina_Bool fail;
   Eina_Value *value;
} PromiseCtx;

typedef struct _Easy_Ctx {
   Eina_Bool success_called;
   Eina_Bool error_called;
   Eina_Bool free_called;
   Eina_Bool stop_loop;
} Easy_Ctx;

typedef struct _Race_Ctx {
   Eina_Value value;
   unsigned int success_idx;
   unsigned int failed;
   unsigned int success;
} Race_Ctx;

typedef struct _Race_Future_Ctx {
   Race_Ctx *race_ctx;
   unsigned int idx;
   int value;
} Race_Future_Ctx;

static void
_cancel(void *data, const Efl_Promise2 *dead_ptr EINA_UNUSED)
{
   PromiseCtx *ctx = data;
   if (ctx->t) ecore_timer_del(ctx->t);
   ctx->t = NULL;
   eina_value_free(ctx->value);
   free(ctx);
}

static void
_promise_cancel_test(void *data, const Efl_Promise2 *dead_ptr EINA_UNUSED)
{
   Eina_Bool *cancel_called = data;
   *cancel_called = EINA_TRUE;
}

static Eina_Bool
_simple_timeout(void *data)
{
   PromiseCtx *ctx = data;

   if (ctx->fail) efl_promise2_reject(ctx->p, DEFAULT_ERROR);
   else
     {
        Eina_Value v;

        fail_if(!eina_value_copy(ctx->value, &v));
        efl_promise2_resolve(ctx->p, v);
        eina_value_free(ctx->value);
     }
   free(ctx);
   return EINA_FALSE;
}

static PromiseCtx *
_promise_ctx_new(void)
{
   PromiseCtx *ctx;
   ctx = calloc(1, sizeof(PromiseCtx));
   fail_if(!ctx);
   ctx->p = efl_promise2_new(_cancel, ctx);
   fail_if(!ctx->p);
   return ctx;
}

static Efl_Future2 *
_future_get(PromiseCtx *ctx, double timeout)
{
   Efl_Future2 *f;

   f = efl_future2_new(ctx->p);
   fail_if(!f);
   ctx->t = ecore_timer_add(timeout, _simple_timeout, ctx);
   fail_if(!ctx->t);
   return f;
}

static Efl_Future2 *
_fail_future_get(void)
{
   PromiseCtx *ctx = _promise_ctx_new();
   ctx->fail = EINA_TRUE;
   return _future_get(ctx, DEFAULT_TIMEOUT);
}


static Efl_Future2 *
_int_future_with_value_and_timeout(int value, double timeout)
{
   PromiseCtx *ctx = _promise_ctx_new();
   ctx->value = eina_value_util_int_new(value);
   fail_if(!ctx->value);
   return _future_get(ctx, timeout);
}

static Efl_Future2 *
_int_future_get(void)
{
   return _int_future_with_value_and_timeout(DEFAULT_INT_VALUE, DEFAULT_TIMEOUT);
}

static Efl_Future2 *
_str_future_get(void)
{
   PromiseCtx *ctx = _promise_ctx_new();
   ctx->value = eina_value_util_string_new(DEFAULT_MSG);
   fail_if(!ctx->value);
   return _future_get(ctx, DEFAULT_TIMEOUT);
}

static Eina_Value
_simple_err(void *data EINA_UNUSED, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   ERROR_CHECK(v, DEFAULT_ERROR);
   ecore_main_loop_quit();
   return v;
}

static Eina_Value
_simple_ok(void *data EINA_UNUSED, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   const char *msg;

   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   fail_if(!eina_value_get(&v, &msg));
   ck_assert_str_eq(DEFAULT_MSG, msg);
   ecore_main_loop_quit();
   return v;
}

static Eina_Value
_chain_stop(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   int *i = data;
   fail_if(*i != CHAIN_SIZE);
   ecore_main_loop_quit();
   return v;
}

static Eina_Value
_chain_no_error(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   Eina_Value new_v;
   static int count = DEFAULT_INT_VALUE;
   int current_i;
   int *i = data;

   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_INT);
   fail_if(!eina_value_get(&v, &current_i));
   fail_if(current_i != count++);
   fail_if(!eina_value_setup(&new_v, EINA_VALUE_TYPE_INT));
   fail_if(!eina_value_set(&new_v, count));
   (*i)++;
   return new_v;
}

static Eina_Value
_chain_error(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   int *i = data;

   ERROR_CHECK(v, DEFAULT_ERROR);
   (*i)++;
   return v;
}

static Eina_Value
_cancel_cb(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   Eina_Value new_v;
   int *cancel_count = data;

   fail_if(!eina_value_setup(&new_v, EINA_VALUE_TYPE_INT));
   ERROR_CHECK(v, ECANCELED);
   (*cancel_count)++;
   /* Although this function returns an INT Eina_Value, the next
      _cancel_cb must receive a EINA_VALYE_TYPE_ERROR as ECANCELED */
   return new_v;
}

static Eina_Value
_inner_resolve(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   Eina_Value new_v;
   fail_if(!eina_value_setup(&new_v, EINA_VALUE_TYPE_STRING));
   fail_if(!eina_value_set(&new_v, DEFAULT_MSG));
   efl_promise2_resolve(data, new_v);
   return v;
}

static Eina_Value
_inner_fail(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   efl_promise2_reject(data, DEFAULT_ERROR);
   return v;
}

static void
_inner_promise_cancel(void *data EINA_UNUSED, const Efl_Promise2 *dead_ptr EINA_UNUSED)
{
   //This must never happen...
   fail_if(EINA_FALSE);
}

static Eina_Value
_future_promise_create(void *data, const Eina_Value v EINA_UNUSED, const Efl_Future2 *dead_future EINA_UNUSED)
{
   Efl_Promise2 *p;

   p = efl_promise2_new(_inner_promise_cancel, NULL);
   fail_if(!p);
   efl_future2_then(_str_future_get(),
                    data ? _inner_fail : _inner_resolve,
                    p);
   return efl_promise2_as_value(p);
}

static Eina_Value
_inner_future_last(void *data, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   if (data)
     ERROR_CHECK(v, DEFAULT_ERROR);
   else
     {
        const char *msg;
        VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
        fail_if(!eina_value_get(&v, &msg));
        ck_assert_str_eq(DEFAULT_MSG, msg);
     }
   ecore_main_loop_quit();
   return v;
}

static Eina_Value
_convert_check(void *data EINA_UNUSED, const Eina_Value v, const Efl_Future2 *dead_future EINA_UNUSED)
{
   const char *number;
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   fail_if(!eina_value_get(&v, &number));
   ck_assert_str_eq(DEFAULT_INT_VALUE_AS_STRING, number);
   ecore_main_loop_quit();
   return v;
}

static Eina_Value
_easy_success(void *data, const Eina_Value v)
{
   Easy_Ctx *ctx = data;
   const char *msg;

   ctx->success_called = EINA_TRUE;
   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
   fail_if(!eina_value_get(&v, &msg));
   ck_assert_str_eq(DEFAULT_MSG, msg);
   return v;
}

static Eina_Value
_easy_error(void *data, const Eina_Error err)
{
   Eina_Value v;
   Easy_Ctx *ctx = data;
   fail_if(err != EINVAL);
   fail_if(!eina_value_setup(&v, EINA_VALUE_TYPE_ERROR));
   fail_if(!eina_value_set(&v, err));
   ctx->error_called = EINA_TRUE;
   return v;
}

static void
_easy_free(void *data, const Efl_Future2 *dead_future EINA_UNUSED)
{
   Easy_Ctx *ctx = data;
   ctx->free_called = EINA_TRUE;
   if (ctx->stop_loop) ecore_main_loop_quit();
}

static Eina_Value
_all_cb(void *data, const Eina_Value array, const Efl_Future2 *dead EINA_UNUSED)
{
   unsigned int len, i, *expected_len = data;

   VALUE_TYPE_CHECK(array, EINA_VALUE_TYPE_ARRAY);
   len = eina_value_array_count(&array);
   fail_if(len != *expected_len);

   for (i = 0; i < len; i++)
     {
        Eina_Value v;

        fail_if(!eina_value_array_get(&array, i, &v));
        if (i % 2 == 0)
          {
             const char *msg;
             VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRING);
             fail_if(!eina_value_get(&v, &msg));
             ck_assert_str_eq(DEFAULT_MSG, msg);
          }
        else
          {
             int ivalue = 0;
             VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_INT);
             fail_if(!eina_value_get(&v, &ivalue));
             fail_if(ivalue != DEFAULT_INT_VALUE);
          }
        eina_value_flush(&v);
     }
   ecore_main_loop_quit();
   return array;
}

static Eina_Value
_future_all_count(void *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
{
   unsigned int *futures_called = data;
   (*futures_called)++;
   return v;
}

static Eina_Value
_race_cb(void *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
{
   Race_Future_Ctx *future_ctx = data;
   Race_Ctx *ctx = future_ctx->race_ctx;

   if (v.type == EINA_VALUE_TYPE_ERROR)
     {
        Eina_Error err;
        eina_value_get(&v, &err);
        fail_if(err != ECANCELED);
        ctx->failed++;
     }
   else if (v.type == EINA_VALUE_TYPE_INT)
     {
        int i;
        fail_if(!eina_value_get(&v, &i));
        fail_if(future_ctx->value != i);
        ctx->success++;
        ctx->success_idx = future_ctx->idx;
        fail_if(!eina_value_copy(&v, &ctx->value));
     }
   else fail_if(EINA_TRUE); //This is not supposed to happen!
   free(future_ctx);
   return v;
}

static Eina_Value
_race_end_cb(void *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
{
   Race_Ctx *ctx = data;
   unsigned int idx;
   Eina_Value_Struct *st;
   Eina_Value r;

   VALUE_TYPE_CHECK(v, EINA_VALUE_TYPE_STRUCT);

   st = eina_value_memory_get(&v);
   fail_if(!st);
   fail_if(st->desc != EFL_PROMISE2_RACE_STRUCT_DESC);
   fail_if(!eina_value_struct_get(&v, "index", &idx));
   fail_if(idx != ctx->success_idx);
   fail_if(!eina_value_struct_get(&v, "value", &r));
   fail_if(eina_value_compare(&r, &ctx->value));
   eina_value_flush(&r);
   ecore_main_loop_quit();
   return v;
}

START_TEST(efl_test_promise_future_success)
{
   Efl_Future2 *f;
   fail_if(!ecore_init());
   f = efl_future2_then(_str_future_get(),
                        _simple_ok, NULL);
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_failure)
{
   Efl_Future2 *f;
   fail_if(!ecore_init());
   f = efl_future2_then(_fail_future_get(),
                        _simple_err, NULL);
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_chain_no_error)
{
   Efl_Future2 *f;
   static int i = 0;

   fail_if(!ecore_init());
   f = efl_future2_chain(_int_future_get(),
                         {.cb = _chain_no_error, .data = &i},
                         {.cb = _chain_no_error, .data = &i},
                         {.cb = _chain_no_error, .data = &i},
                         {.cb = _chain_stop, .data = &i});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_chain_error)
{
   Efl_Future2 *f;
   static int i = 0;

   fail_if(!ecore_init());
   f = efl_future2_chain(_fail_future_get(),
                         {.cb = _chain_error, .data = &i},
                         {.cb = _chain_error, .data = &i},
                         {.cb = _chain_error, .data = &i},
                         {.cb = _chain_stop, .data = &i});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
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
   Efl_Future2 *f;

   fail_if(!ecore_init());
   f = efl_future2_chain(_str_future_get(),
                         {.cb = _future_promise_create, .data = NULL},
                         {.cb = _inner_future_last, .data = NULL});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_inner_promise_fail)
{
   Efl_Future2 *f;
   void *data =(void *) 0x01;

   fail_if(!ecore_init());
   f = efl_future2_chain(_str_future_get(),
                         {.cb = _future_promise_create, .data = data},
                         {.cb = _inner_future_last, .data = data});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
}
END_TEST

START_TEST(efl_test_promise_future_implicit_cancel)
{
   Efl_Promise2 *p;
   Efl_Future2 *f;
   int cancel_count = 0;
   Eina_Bool cancel_called = EINA_FALSE;
   Eina_Value v = EINA_VALUE_EMPTY;

   fail_if(!ecore_init());

   p = efl_promise2_new(_promise_cancel_test, &cancel_called);
   fail_if(!p);
   f = efl_future2_new(p);
   fail_if(!f);
   f = efl_future2_chain(f,
                         {.cb = _cancel_cb, .data = &cancel_count},
                         {.cb = _cancel_cb, .data = &cancel_count},
                         {.cb = _cancel_cb, .data = &cancel_count});
   fail_if(!f);
   efl_promise2_resolve(p, v);
   /*
     The promise was resolved, but the mainloop is not running.
     Since ecore_shutdown() will be called all the futures must be cancelled
   */
   ecore_shutdown();
   //All the futures were cancelled at this point
   fail_if(cancel_count != CHAIN_SIZE);
   //Cancel should not be called, since we called efl_promise2_resolve()
   fail_if(cancel_called);
}
END_TEST

START_TEST(efl_test_promise_future_convert)
{
   Efl_Future2 *f;

   fail_if(!ecore_init());
   f = efl_future2_chain(_int_future_get(),
                         efl_future2_cb_convert_to(EINA_VALUE_TYPE_STRING),
                         { .cb = _convert_check, .data = NULL });
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();

}
END_TEST

START_TEST(efl_test_promise_future_easy)
{
   Efl_Future2 *f;
   Easy_Ctx easy1 = { 0 };
   Easy_Ctx easy2 = { 0 };
   Easy_Ctx easy3 = { 0 };

   easy3.stop_loop = EINA_TRUE;
   fail_if(!ecore_init());
   f = efl_future2_then_from_desc(_str_future_get(),
                                  efl_future2_cb_easy(_easy_success,
                                                      _easy_error,
                                                      _easy_free,
                                                      EINA_VALUE_TYPE_STRING,
                                                      &easy1));
   fail_if(!f);
   f = efl_future2_then_easy(f, _easy_success, _easy_error,
                             _easy_free, NULL, &easy2);
   fail_if(!f);
   f = efl_future2_chain_easy(f, {_easy_success, _easy_error,
          _easy_free, EINA_VALUE_TYPE_INT, &easy3});
   fail_if(!f);
   ecore_main_loop_begin();
   ecore_shutdown();
   fail_if(!(easy1.success_called && !easy1.error_called && easy1.free_called));
   fail_if(!(easy2.success_called && !easy2.error_called && easy2.free_called));
   fail_if(!(!easy3.success_called && easy3.error_called && easy3.free_called));
}
END_TEST

START_TEST(efl_test_promise_future_all)
{
   Efl_Future2 *futures[11];
   unsigned int i, futures_called = 0, len = EINA_C_ARRAY_LENGTH(futures);

   fail_if(!ecore_init());
   for (i = 0; i < len - 1; i++)
     {
        Efl_Future2 *f;
        if (i % 2 == 0)
          f = _str_future_get();
        else
          f = _int_future_get();
        fail_if(!f);
        futures[i] = efl_future2_then(f, _future_all_count, &futures_called);
        fail_if(!futures[i]);
     }

   futures[--len] = NULL;
   fail_if(!efl_future2_then(efl_future2_all_array(futures), _all_cb, &len));
   ecore_main_loop_begin();
   ecore_shutdown();
   fail_if(futures_called != len);
}
END_TEST

START_TEST(efl_test_promise_future_race)
{
   Race_Ctx ctx = { 0 };
   Efl_Future2 *futures[11];
   unsigned int i, len = EINA_C_ARRAY_LENGTH(futures);
   double timeouts[10] = {
     2.0, 1.0, 0.5, 0.1, 4.5, 2.3, 5.6, 1.0, 0.5, 0.3
   };

   srand(time(NULL));
   fail_if(!ecore_init());
   for (i = 0; i < len - 1; i++)
     {
        Race_Future_Ctx *future_ctx = calloc(1, sizeof(Race_Future_Ctx));
        fail_if(!future_ctx);
        future_ctx->race_ctx = &ctx;
        future_ctx->idx = i;
        future_ctx->value = rand() % RAND_MAX;
        futures[i] = efl_future2_then(_int_future_with_value_and_timeout(future_ctx->value, timeouts[i]),
                                      _race_cb, future_ctx);
        fail_if(!futures[i]);
     }

   futures[--len] = NULL;
   fail_if(!efl_future2_then(efl_future2_race_array(futures),
                             _race_end_cb, &ctx));
   ecore_main_loop_begin();
   eina_value_flush(&ctx.value);
   ecore_shutdown();
   fail_if(ctx.success != 1);
   fail_if(ctx.failed != (len - 1));
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
   tcase_add_test(tc, efl_test_promise_future_convert);
   tcase_add_test(tc, efl_test_promise_future_easy);
   tcase_add_test(tc, efl_test_promise_future_all);
   tcase_add_test(tc, efl_test_promise_future_race);
}

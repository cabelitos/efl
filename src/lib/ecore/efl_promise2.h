#ifndef _EFL_PROMISE2_H_
#define _EFL_PROMISE2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Eina.h>

typedef struct _Efl_Future2_Desc Efl_Future2_Desc;
typedef struct _Efl_Promise2 Efl_Promise2;
typedef struct _Efl_Future2 Efl_Future2;
typedef struct _Efl_Future2_Desc Efl_Future2_Desc;
typedef Eina_Value (*Efl_Future2_Cb)(void *data, const Eina_Value value, const Efl_Future2 *dead_future);
typedef void (*Efl_Promise2_Cancel_Cb) (void *data, const Efl_Promise2 *dead_promise);
typedef Eina_Value (*Efl_Future2_Success_Cb)(void *data, const Eina_Value value);
typedef Eina_Value (*Efl_Future2_Error_Cb)(void *data, const Eina_Error error);
typedef void (*Efl_Future2_Free_Cb)(void *data, const Efl_Future2 *dead_future);

typedef struct _Efl_Future2_Cb_Easy_Desc {
   /**
    * called on success (value.type is not @c EINA_VALUE_TYPE_ERROR).
    *
    * if @c success_type is not NULL, then the value is guaranteed to be of that type,
    * if it's not, then it will trigger @c error with @c EINVAL.
    *
    * After this function returns, @c free callback is called if provided.
    */
   Efl_Future2_Success_Cb success;
   /**
    * called on error (value.type is @c EINA_VALUE_TYPE_ERROR).
    *
    * This function can return another error, propagating or converting it. However it
    * may also return a non-error, in this case the next future in chain will receive a regular
    * value, which may call its @c success.
    *
    * If this function is not provided, then it will pass thru the error to the next error handler.
    *
    * It may be called with @c EINVAL if @c success_type is provided and doesn't
    * match the received type.
    *
    * It may be called with @c ECANCELED if future was canceled.
    *
    * It may be called with @c ENOMEM if memory allocation failed during callback creation.
    *
    * After this function returns, @c free callback is called if provided.
    */
   Efl_Future2_Error_Cb error;
   /**
    * called on @b all situations to notify future destruction.
    *
    * This is called after @c success or @c error, as well as it's called if none of them are
    * provided. Thus can be used as a "weak ref" mechanism.
    */
   Efl_Future2_Free_Cb free;
   /**
    * If provided, then @c success will only be called if the value type matches the given pointer.
    *
    * If provided and doesn't match, then @c error will be called with @c EINVAL. If no @c error,
    * then it will be propagated to the next future in the chain.
    */
   const Eina_Value_Type *success_type;
   /**
    * context data given to every callback.
    *
    * This must be freed @b only by @c free callback as it's called from every case,
    * otherwise it may lead to memory leaks.
    */
   const void *data;
} Efl_Future2_Cb_Easy_Desc;

struct _Efl_Future2_Desc {
   Efl_Future2_Cb cb;
   const void *data;
};

EAPI Efl_Promise2 *efl_promise2_new(Efl_Promise2_Cancel_Cb cancel_cb, const void *data);
EAPI void efl_promise2_resolve(Efl_Promise2 *p, Eina_Value value);
EAPI void efl_promise2_reject(Efl_Promise2 *p, Eina_Error err);
EAPI void efl_future2_cancel(Efl_Future2 *f);
EAPI Eina_Value efl_promise2_as_value(Efl_Promise2 *p);
EAPI Eina_Value efl_future2_as_value(Efl_Future2 *f);
EAPI Efl_Future2 *efl_future2_new(Efl_Promise2 *p);
EAPI Efl_Future2 *efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc);
EAPI Efl_Future2 *efl_future2_chain_array(Efl_Future2 *prev, const Efl_Future2_Desc descs[]);
EAPI Efl_Future2_Desc efl_future2_cb_console(const char *prefix, const char *suffix);
EAPI Efl_Future2_Desc efl_future2_cb_convert_to(const Eina_Value_Type *type);
EAPI void *efl_promise2_data_get(const Efl_Promise2 *p) EINA_ARG_NONNULL(1);
EAPI Efl_Future2_Desc efl_future2_cb_easy_from_desc(const Efl_Future2_Cb_Easy_Desc desc);

#ifndef __cplusplus
#define efl_future2_cb_easy(...) efl_future2_cb_easy_from_desc((Efl_Future2_Cb_Easy_Desc){__VA_ARGS__})
#define efl_future2_chain(_prev, ...) efl_future2_chain_array(_prev, (Efl_Future2_Desc[]){__VA_ARGS__, {.cb = NULL, .data = NULL}})
#define efl_future2_then(_prev, ...) efl_future2_then_from_desc(_prev, (Efl_Future2_Desc){__VA_ARGS__})
#endif

#ifdef __cplusplus
}
#endif
#endif

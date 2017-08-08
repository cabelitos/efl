#ifndef _EFL_PROMISE2_H_
#define _EFL_PROMISE2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Eina.h>

/**
 * @ingroup Efl_Promise2
 *
 * @{
 */
typedef struct _Efl_Future2_Desc Efl_Future2_Desc;
typedef struct _Efl_Promise2 Efl_Promise2;
typedef struct _Efl_Future2 Efl_Future2;
typedef struct _Efl_Future2_Desc Efl_Future2_Desc;
typedef struct _Efl_Future2_Cb_Easy_Desc Efl_Future2_Cb_Easy_Desc;

/**
 * @typedef Efl_Future2_Cb Efl_Future2_Cb
 *
 * A callback used to inform that a future was resolved.
 * Usually this callback is called from a clean context, that is, from the
 * main loop or some platform defined safe context. However there are
 * 2 exceptions:
 *
 * @li efl_future2_cancel() was used, it's called immediately in the
 * context that called cancel using `ECANCELED` as error.
 *
 * @li efl_future2_then(), efl_future2_then_from_desc(), efl_future2_chain(), efl_future2_chain_array()
 * or similar failed due invalid pointer or memory allocation. Then the callback is called from the
 * failed context using `EINVAL` or `ENOMEM` as errors and @p dead_future will be @c NULL.
 *
 * @param data The data provided by the user
 *
 * @param value An Eina_Value which contains the operation result. Before using
 * the @p value, its type must be checked in order to avoid errors. This is needed, because
 * if an operation fails the Eina_Value type will be EINA_VALUE_TYPE_ERROR
 * which is a different type than the expected operation result.
 *
 * @param dead_future A pointer to the future that was completed.
 *
 * @return An Eina_Value to pass to the next Efl_Future2 in the chain (if any).
 * If there is no need to convert the received value, it's @b recommended
 * to pass-thru @p value argument. If you need to convert to a different type
 * or generate a new value, use @c eina_value_setup() on @b another Eina_Value
 * and return it. By returning an promise Eina_Value (efl_promise2_as_value()) the
 * whole chain will wait until the promise is resolved in
 * order to continue its execution.
 * Note that the value contents must survive this function scope,
 * that is, do @b not use stack allocated blobs, arrays, structures or types that
 * keeps references to memory you give. Values will be automatically cleaned up
 * using @c eina_value_flush() once they are unused (no more future or futures
 * returned a new value).
 *
 * @note The returned value @b can be an EFL_VALUE_TYPE_PROMISE2! (see efl_promise2_as_value() and
 * efl_future2_as_value()) In this case the future chain will wait until the promise is resolved.
 *
 * @see efl_future2_cancel()
 * @see efl_future2_then()
 * @see efl_future2_then_from_desc()
 * @see efl_future2_chain()
 * @see efl_future2_chain_array()
 * @see efl_future2_as_value()
 * @see efl_promise2_as_value()
 */
typedef Eina_Value (*Efl_Future2_Cb)(void *data, const Eina_Value value, const Efl_Future2 *dead_future);

/**
 * @typedef Efl_Promise2_Cancel_Cb Efl_Promise2_Cancel_Cb.
 *
 * A callback used to inform that a promise was canceled.
 *
 * A promise may be canceled by the user calling `efl_future2_cancel()`
 * on any Efl_Future2 that is part of the chain that uses an Efl_Promise2,
 * that will cancel the whole chain and then the promise.
 *
 * It should stop all asynchronous operations or at least mark them to be
 * discarded instead of resolved. Actually it can't be resolved once
 * cancelled since the given pointer @c dead_promise is now invalid.
 *
 * @note This callback is @b mandatory for a reason, do not provide an empty
 *       callback as it'll likely result in memory corruption and invalid access.
 *       If impossible to cancel an asynchronous task, then create an
 *       intermediate memory to hold Efl_Promise2 and make it @c NULL,
 *       in this callback. Then prior to resolution check if the pointer is set.
 *
 * @note This callback is @b not called if efl_promise2_resolve() or
 *       efl_promise2_reject() are used. If any cleanup is needed, then
 *       call yourself. It's only meant as cancellation, not a general
 *       "free callback".
 *
 * @param data The data provided by the user.
 * @param dead_promise The canceled promise.
 * @see efl_promise2_reject()
 * @see efl_promise2_resolve()
 * @see efl_future2_cancel()
 */
typedef void (*Efl_Promise2_Cancel_Cb) (void *data, const Efl_Promise2 *dead_promise);

/**
 * @typedef Efl_Future2_Success_Cb Efl_Future2_Success_Cb.
 *
 * A callback used to inform that the future completed with success.
 *
 * Unlike #Efl_Future2_Cb this callback only called if the future operation was successful, this is,
 * no errors occurred (@p value type differs from EINA_VALUE_TYPE_ERROR)
 * and the @p value matches #_Efl_Future2_Cb_Easy_Desc::success_type (if given).
 * In case #_Efl_Future2_Cb_Easy_Desc::success_type was supplied the @p value type must be
 * before using it.
 *
 * @note This function is always called from a safe context (main loop or some platform defined safe context).
 *
 * @param data The data provided by the user.
 * @param value The operation result
 * @return An Eina_Value to pass to the next Efl_Future2 in the chain (if any).
 * If there is no need to convert the received value, it's @b recommended
 * to pass-thru @p value argument. If you need to convert to a different type
 * or generate a new value, use @c eina_value_setup() on @b another Eina_Value
 * and return it. By returning an promise Eina_Value (efl_promise2_as_value()) the
 * whole chain will wait until the promise is resolved in
 * order to continue its execution.
 * Note that the value contents must survive this function scope,
 * that is, do @b not use stack allocated blobs, arrays, structures or types that
 * keeps references to memory you give. Values will be automatically cleaned up
 * using @c eina_value_flush() once they are unused (no more future or futures
 * returned a new value).
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 */
typedef Eina_Value (*Efl_Future2_Success_Cb)(void *data, const Eina_Value value);

/**
 * @typedef Efl_Future2_Error_Cb Efl_Future2_Error_Cb.
 *
 * A callback used to inform that the future completed with failure.
 *
 * Unlike #Efl_Future2_Success_Cb this function is only called when an error
 * occurs during the future process or when #_Efl_Future2_Cb_Easy_Desc::success_type
 * differs from the future result.
 * On future creation errors and future cancellation this function will be called
 * from the current context with the following errors respectitally: `EINVAL`, `ENOMEM` and  `ECANCELED`.
 * Otherwise this function is called from a safe context.
 *
 * If it was possible to recover from an error this function should return an empty value
 * `return (Eina_Value) {0};` or any other Eina_Value type that differs from EINA_VALUE_TYPE_ERROR.
 * In this case the error will not be reported by the other futures in the chain (if any), otherwise
 * if an Eina_Value type EINA_VALUE_TYPE_ERROR is returned the error will continue to be reported
 * to the other futures in the chain.
 *
 * @param data The data provided by the user.
 * @param error The operation error
 * @return An Eina_Value to pass to the next Efl_Future2 in the chain (if any).
 * If you need to convert to a different type or generate a new value,
 * use @c eina_value_setup() on @b another Eina_Value
 * and return it. By returning an promise Eina_Value (efl_promise2_as_value()) the
 * whole chain will wait until the promise is resolved in
 * order to continue its execution.
 * Note that the value contents must survive this function scope,
 * that is, do @b not use stack allocated blobs, arrays, structures or types that
 * keeps references to memory you give. Values will be automatically cleaned up
 * using @c eina_value_flush() once they are unused (no more future or futures
 * returned a new value).
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 */
typedef Eina_Value (*Efl_Future2_Error_Cb)(void *data, const Eina_Error error);

/**
 * @typedef Efl_Future2_Free_Cb Efl_Future2_Free_Cb.
 *
 * A callback used to inform that the future was freed and the user should also @c free the @p data.
 *
 * @note This callback is always called, even if #Efl_Future2_Error_Cb and/or #Efl_Future2_Success_Cb
 * were not provided, which can also be used to monitor when a future ends.
 *
 * @param data The data provided by the user.
 * @param dead_future The future that will be freed.
 *
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 */
typedef void (*Efl_Future2_Free_Cb)(void *data, const Efl_Future2 *dead_future);

/**
 * @struct _Efl_Future2_Cb_Easy_Desc
 *
 * A struct with callbacks to be used by efl_future2_cb_easy_from_desc() and efl_future2_cb_easy()
 *
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 */
struct _Efl_Future2_Cb_Easy_Desc {
   /**
    * Called on success (value.type is not @c EINA_VALUE_TYPE_ERROR).
    *
    * if @c success_type is not NULL, then the value is guaranteed to be of that type,
    * if it's not, then it will trigger @c error with @c EINVAL.
    *
    * After this function returns, @c free callback is called if provided.
    */
   Efl_Future2_Success_Cb success;
   /**
    * Called on error (value.type is @c EINA_VALUE_TYPE_ERROR).
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
    * Called on @b all situations to notify future destruction.
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
    * Context data given to every callback.
    *
    * This must be freed @b only by @c free callback as it's called from every case,
    * otherwise it may lead to memory leaks.
    */
   const void *data;
};

/**
 * @struct _Efl_Future2_Desc
 *
 * A struct used to define a callback and data for a future.
 *
 * This struct contains a future completion callback and a data to the future
 * completion callback which is used by efl_future2_then(), efl_future2_chain()
 * and friends to inform the user about the future result. The #_Efl_Future2_Desc::data
 * variable should be freed when #_Efl_Future2_Desc::cb is called, otherwise it will leak.
 * If efl_future2_then(), efl_future2_chain() and friends fails to return a valid future
 * (in other words: @c NULL is returned) the #_Efl_Future2_Desc::cb will be called
 * report an error like `EINVAL` or `ENOMEM` so #_Efl_Future2_Desc::data can be freed.
 *
 * @see efl_future2_then()
 * @see efl_future2_chain_array()
 * @see efl_future2_cb_convert_to()
 * @see efl_future2_cb_console()
 * @see efl_future2_cb_easy_from_desc()
 */
struct _Efl_Future2_Desc {
   /**
    * Called when the future is resolved or rejected.
    *
    * Once a future is resolved or rejected this function is called passing the future result
    * to inform the user that the future operation has ended. Normally this
    * function is called from a safe context (main loop or some platform defined safe context),
    * however in case of a future cancellation (efl_future2_cancel()) or if efl_future2_then(),
    * efl_future2_chain() and friends fails to create a new future,
    * this function is called from the current context.
    *
    * Use this function to free @p data if necessary.
    * @see efl_future2_chain()
    * @see efl_future2_then()
    * @see efl_future2_cancel()
    */
   Efl_Future2_Cb cb;
   /**
    * Context data to @p cb. The @p data should be freed inside @p cb, otherwise
    * its memory will leak!
    */
   const void *data;
};

/**
 * Creates a new promise.
 *
 * This function creates a new promise which can be used to create a future
 * using efl_future2_new(). Everytime a promise is created a #Efl_Promise2_Cancel_Cb
 * must be provided which is used to free resources that were created.
 *
 * A promise may be cancelled directly or indirectly:
 *
 * <table>
 * <tr><th>Cancel Reason</th>Indirectly canceled</th>
 * <tr><td>A future in the future chain is cancelled.</td><td>True</td></tr>
 * <tr><td>Subsystem shutdown (ecore_shutdown())</td><td>True</td></tr>
 * <tr><td>When an EO object links a future to its life cycle and the EO object
 * is deleted, thus cancelling the future.</td><td>True</td></tr>
 * <tr><td>The future linked against the promise is canceled.Example: `efl_future2_cancel((efl_future2_new(efl_promise2_new(...))))`</td><td>False</td></tr>
 * </table>
 *
 * Since a promise may be canceled indirectaly (by code sections that goes beyond your scope)
 * you should always provide a cancel callback, even if you think you'll not need it.
 *
 * Below there's a typical example:
 *
 * @code
 * #include <Ecore.h>
 *
 * static void
 * _promise_cancel(void *data, Efl_Promise2 *p EINA_UNUSED)
 * {
 *   Ctx *ctx = data;
 *   //In case the promise is canceled we must stop the timer!
 *   ecore_timer_del(ctx->timer);
 *   free(ctx);
 * }
 *
 * static Eina_Bool
 * _promise_resolve(void *data)
 * {
 *    Ctx *ctx = data;
 *    Eina_Value v;
 *    eina_value_setup(&v, EINA_VALUE_TYPE_STRING);
 *    eina_value_set(&v, "Promise resolved");
 *    efl_promise2_resolve(ctx->p, v);
 *    free(ctx);
 *    return EINA_FALSE;
 * }
 *
 * Efl_Promise2 *
 * promise_create(void)
 * {
 *    Ctx *ctx = malloc(sizeof(Ctx));
 *    //A timer is scheduled in order to resolve the promise
 *    ctx->timer = ecore_timer_add(122, _promise_resolve, ctx);
 *    //The _promise_cancel() will be used to clean ctx if the promise is canceled.
 *    ctx->p = efl_promise2_new(_promise_cancel, ctx);
 *    return ctx->p;
 * }
 * @endcode
 *
 * @param cancel_cb A callback used to inform that the promise was canceled. Use
 * this callback to @c free @p data. @p cancel_cb must not be @c NULL !
 * @param data Data to @p cancel_cb.
 * @return A promise or @c NULL on error.
 * @see efl_future2_cancel()
 * @see efl_future2_new()
 * @see efl_promise2_resolve()
 * @see efl_promise2_reject()
 * @see efl_promise2_data_get()
 * @see efl_promise2_as_value()
 */
EAPI Efl_Promise2 *efl_promise2_new(Efl_Promise2_Cancel_Cb cancel_cb, const void *data);
/**
 * Resolves a promise.
 *
 *
 * This function schedules an resolve event in a
 * safe context (main loop or some platform defined safe context),
 * whenever possible the future callbacks will be dispatched.
 *
 * @param p A promise to resolve.
 * @param value The value to be delivered. Note that the value contents must survive this function scope,
 * that is, do @b not use stack allocated blobs, arrays, structures or types that
 * keeps references to memory you give. Values will be automatically cleaned up
 * using @c eina_value_flush() once they are unused (no more future or futures
 * returned a new value).
 *
 * @see efl_promise2_new()
 * @see efl_promise2_reject()
 * @see efl_promise2_data_get()
 * @see efl_promise2_as_value()
 */
EAPI void efl_promise2_resolve(Efl_Promise2 *p, Eina_Value value);
/**
 * Rejects a promise.
 *
 * This function schedules an reject event in a
 * safe context (main loop or some platform defined safe context),
 * whenever possible the future callbacks will be dispatched.
 *
 * @param p A promise to reject.
 * @param err An Eina_Error value
 * @note Internally this function will create an Eina_Value with type #EINA_VALUE_TYPE_ERROR.
 *
 * @see efl_promise2_new()
 * @see efl_promise2_resolve()
 * @see efl_promise2_data_get()
 * @see efl_promise2_as_value()
 */
EAPI void efl_promise2_reject(Efl_Promise2 *p, Eina_Error err);
/**
 * Cancels a future.
 *
 * This function will cancel the whole future chain immediately (it will not be schedule to the next mainloop pass)
 * and it will also cancel the promise linked against it. The Efl_Future2_Cb will be called
 * with an Eina_Value typed as #EINA_VALUE_TYPE_ERROR, which its value will be
 * ECANCELED
 * @param f The future to cancel.
 */
EAPI void efl_future2_cancel(Efl_Future2 *f);
/**
 * Creates a new Eina_Value from a promise.
 *
 * This function creates a new Eina_Value that will store a promise
 * in it. This function is useful for dealing with promises inside
 * a #Efl_Future2_Cb. By returning an Promise Eina_Value the
 * whole chain will wait until the promise is resolved in
 * order to continue its execution. Example:
 *
 * @code
 * static Eina_Value
 * _file_data_ready(const *data EINA_UNUSED, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
 * {
 *    const char *file_data;
 *    Efl_Promise2 *p;
 *    //It was not possible to fetch the file size.
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *    {
 *       Eina_Error err;
 *       eina_value_get(&v, &err);
 *       fprintf(stderr, "Could get the file data. Reason: %s\n", eina_error_msg_get(err));
 *       ecore_main_loop_quit();
 *       return v;
 *    }
 *    if (v.type != EINA_VALUE_TYPE_STRING)
 *    {
 *      fprintf(stderr, "Expecting type '%s' - received '%s'\n", EINA_VALUE_TYPE_STRING->name, v.type.name);
 *      ecore_main_loop_quit();
 *      return v;
 *    }
 *
 *    eina_value_get(&v, &file_data);
 *    //count_words will count the words in the background and resolve the promise once it is over...
 *    p = count_words(file_data);
 *    return efl_promise2_as_value(p);
 * }
 *
 * static Eina_Value
 * _word_count_ready(const *data EINA_UNUSED, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
 * {
 *    //The _word_count_ready will only be called once count_words() resolves/rejects the promise returned by _file_data_ready()
 *    int count;
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *    {
 *       Eina_Error err;
 *       eina_value_get(&v, &err);
 *       fprintf(stderr, "Could not count the file words. Reason: %s\n", eina_error_msg_get(err));
 *       ecore_main_loop_quit();
 *       return v;
 *    }
 *    if (v.type != EINA_VALUE_TYPE_INT)
 *    {
 *      fprintf(stderr, "Expecting type '%s' - received '%s'\n", EINA_VALUE_TYPE_INT->name, v.type.name);
 *      ecore_main_loop_quit();
 *      return v;
 *    }
 *    eina_value_get(&v, &count);
 *    printf("File word count %d\n", count);
 *    return v;
 * }
 *
 * void
 * file_operation(void)
 * {
 *    Efl_Future2 *f = get_file_data("/MyFile.txt");
 *    efl_future2_chain(f, {.cb = _file_data_ready, .data = NULL},
 *                         {.cb = _word_count_ready, .data = NULL});
 * }
 * @endcode
 *
 * @return An Eina_Value. On error the value's type will be @c NULL.
 * @note If an error happens the promise will be CANCELED.
 * @see efl_future2_as_value()
 * @see efl_promise2_reject()
 * @see efl_promise2_resolve()
 */
EAPI Eina_Value efl_promise2_as_value(Efl_Promise2 *p);
/**
 * Creates an Eina_Value from a future.
 *
 * This function is used for the same purposes as efl_promise2_as_value(),
 * but receives an Efl_Future2 instead.
 *
 * @param f A future to create a Eina_Value from.
 * @return An Eina_Value. On error the value's type will be @c NULL.
 * @note If an error happens the future @p f will be CANCELED
 * @see efl_promise2_as_value()
 */
EAPI Eina_Value efl_future2_as_value(Efl_Future2 *f);

/**
 * Creates a new future.
 *
 * This function creates a new future and can be used to report
 * that an operation has succeded or failed using
 * efl_promise2_resolve() or efl_promise2_reject().
 *
 * Futures can also be canceled using efl_future2_cancel(), which
 * will cause the whole chain to be cancelled alongside with any pending promise.
 *
 * @param p A promise used to attach a future. May not be @c NULL.
 * @return The future or @c NULL on error.
 * @note If an error happens this function will CANCEL the promise.
 * @see efl_promise2_new()
 * @see efl_promise2_reject()
 * @see efl_promise2_resolve()
 * @see efl_future2_cancel()
 */
EAPI Efl_Future2 *efl_future2_new(Efl_Promise2 *p);
/**
 * Register an Efl_Future2_Desc to be used when the future is resolve/rejected.
 *
 * With this function a callback and data is attached to the future and then
 * once it's resolved or rejected the callback will be informed.
 *
 * If during the future creation an error happens this function will return @c NULL,
 * and the #Efl_Future2_Desc::cb will be called reporting an error (`EINVAL` or `ENOMEM`),
 * so the user has a chance to free #Efl_Future2_Desc::data.
 *
 * In case a future in the chain is canceled, the whole chain will be canceled immediately
 * and the error `ECANCELED` will be reported.
 *
 * Below there's a simple usage of this function.
 *
 * @code
 * static Eina_Value
 * _file_ready(const *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
 * {
 *    Ctx *ctx = data;
 *    //It was not possible to fetch the file size.
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *    {
 *       Eina_Error err;
 *       eina_value_get(&v, &err);
 *       fprintf(stderr, "Could not read the file size. Reason: %s\n", eina_error_msg_get(err));
 *       ecore_main_loop_quit();
 *       return v;
 *    }
 *    if (v.type != EINA_VALUE_TYPE_INT)
 *    {
 *      fprintf(stderr, "Expecting type '%s' - received '%s'\n", EINA_VALUE_TYPE_INT->name, v.type.name);
 *      ecore_main_loop_quit();
 *      return v;
 *    }
 *    eina_value_get(&v, &ctx->size);
 *    printf("File size is %d bytes\n", ctx->size);
 *    return v;
 * }
 *
 * void
 * file_operation(void)
 * {
 *    Efl_Future2 *f = get_file_size_async("/MyFile.txt");
 *    efl_future2_then_from_desc(f, (const Efl_Future2_Desc){.cb = _size_ready, .data = NULL});
 *    //There's a helper macro called efl_future2_then() which simplifies the usage.
 *    //The code below has the same effect.
 *    //efl_future2_then(f, _size_ready, NULL);
 * }
 * @endcode
 *
 * Although the code presented at `_size_ready()` is very simple, most of it
 * is just used to check the Eina_Value type. In order
 * to avoid this type of code the function efl_future2_cb_easy_from_desc()
 * was created. Please, check its documentation for more information.
 *
 * This function can also be used to create a future chain, making
 * it possible to execute the future result in a cascade order. When
 * using a future chain the Eina_Value returned by a #Efl_Future2_Desc::cb
 * will be delivered to the next #Efl_Future2_Desc::cb in the chain.
 *
 * Here's an example:
 *
 * static Eina_Value
 * _future_cb1(const *data EINA_UNUSED, const Eina_Value v)
 * {
 *    Eina_Value new_v;
 *    int i;
 *
 *    //There's no need to check the Eina_Value type since we're using efl_future2_cb_easy()
 *    eina_value_get(&v, &i);
 *    printf("File size as int: %d\n", i);
 *    eina_value_setup(&new_v, EINA_VALUE_TYPE_STRING);
 *    //Convert the file size to string
 *    eina_value_convert(&v, &new_v);
 *    return new_v;
 * }
 *
 * static Eina_Value
 * _future_cb2(const *data EINA_UNUSED, const Eina_Value v)
 * {
 *    Eina_Value new_v;
 *    const char *file_size_str;
 *
 *    //There's no need to check the Eina_Value type since we're using efl_future2_cb_easy()
 *    eina_value_get(&v, &file_size_str);
 *    printf("File size as string: %s\n", file_size_str);
 *    eina_value_setup(&new_v, EINA_VALUE_TYPE_DOUBLE);
 *    eina_value_convert(&v, &new_v);
 *    return new_v;
 * }
 *
 * static Eina_Value
 * _future_cb3(const *data EINA_UNUSED, const Eina_Value v)
 * {
 *    double d;
 *
 *    //There's no need to check the Eina_Value type since we're using efl_future2_cb_easy()
 *    eina_value_get(&v, &d);
 *    printf("File size as double: %g\n", d);
 *    return v;
 * }
 *
 * static Eina_Value
 * _future_err(void *data EINA_UNUSED, Eina_Error err)
 * {
 *    //This function is called if future result type does not match or another error occurred
 *    Eina_Value new_v;
 *    eina_value_setup(&new_v, EINA_VALUE_TYPE_ERROR);
 *    eina_valuse_set(&new_v, err);
 *    fprintf(stderr, "Error during future process. Reason: %s\n", eina_error_msg_get(err));
 *    //Pass the error to the next future in the chain..
 *    return new_v;
 * }
 *
 * @code
 * void chain(void)
 * {
 *   Efl_Future2 *f = get_file_size_async("/MyFile.txt");
 *   f = efl_future2_then(f, efl_future2_cb_easy(_future_cb1, _future_err, NULL, EINA_VALUE_TYPE_INT, NULL));
 *   //_future_cb2 will be executed after _future_cb1()
 *   f = efl_future2_then(f, efl_future2_cb_easy(_future_cb2, _future_err, NULL, EINA_VALUE_TYPE_STRING, NULL));
 *   //_future_cb2 will be executed after _future_cb2()
 *   efl_future2_then(f, efl_future2_cb_easy(_future_cb3, _future_err, NULL, EINA_VALUE_TYPE_DOUBLE, NULL));
 * }
 * @endcode
 *
 * Although it's possible to create a future chain using efl_future2_then()/efl_future2_then_from_desc()
 * there's a function that makes this task much easier, please check efl_future2_chain_array() for more
 * information.
 *
 * @param prev A future to link against
 * @param desc A description struct contaning the callback and data.
 * @return A new future or @c NULL on error.
 * @note If an error happens the whole future chain will CANCELED and
 * desc.cb will be called in order to free desc.data.
 * @see efl_future2_new()
 * @see efl_future2_then()
 * @see #Efl_Future2_Desc
 * @see efl_future2_chain_array()
 * @see efl_future2_chain()
 * @see efl_future2_cb_console()
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 * @see efl_future2_cb_convert_to()
 * @see efl_future2_cancel()
 */
EAPI Efl_Future2 *efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc);
/**
 * Creates a future chain.
 *
 *
 * This behaves exactly like efl_future2_then_from_desc(), but makes it easier to create future chains.
 *
 * If during the future chain creation an error happens this function will return @c NULL,
 * and the #Efl_Future2_Desc::cb from the @p descs array will be called reporting an error (`EINVAL` or `ENOMEM`),
 * so the user has a chance to free #Efl_Future2_Desc::data.
 *
 * Just like efl_future2_then_from_desc(), the value returned by a #Efl_Future2_Desc::cb callback will
 * be delivered to the next #Efl_Future2_Desc::cb in the chain.
 *
 * In case a future in the chain is canceled, the whole chain will be canceled immediately
 * and the error `ECANCELED` will be reported.
 *
 * Here's an example:
 *
 * @code
 *
 * //callbacks code....
 *
 * Efl_Future2* chain(void)
 * {
 *   Efl_Future2 *f = get_file_size_async("/MyFile.txt");
 *   return efl_future2_chain(f, efl_future2_cb_easy(_future_cb1, _future_err, NULL, EINA_VALUE_TYPE_INT, NULL),
 *                               efl_future2_cb_easy(_future_cb2, _future_err, NULL, EINA_VALUE_TYPE_STRING, NULL),
 *                               efl_future2_cb_easy(_future_cb3, _future_err, NULL, EINA_VALUE_TYPE_DOUBLE, NULL),
 *                               {.cb = _future_cb4, .data = NULL });
 * }
 * @encode
 *
 * @param prev The previous future
 * @param descs An array of descriptions. The last element of the array must have the #Efl_Future2_Desc::cb set to @c NULL
 * @return A future or @c NULL on error.
 * @note If an error happens the whole future chain will CANCELED and
 * desc.cb will be called in order to free desc.data.
 * @see efl_future2_new()
 * @see efl_future2_then()
 * @see #Efl_Future2_Desc
 * @see efl_future2_chain()
 * @see efl_future2_cb_console()
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 * @see efl_future2_then_from_desc()
 * @see efl_future2_cb_convert_to()
 */
EAPI Efl_Future2 *efl_future2_chain_array(Efl_Future2 *prev, const Efl_Future2_Desc descs[]);
/**
 * Creates an Efl_Future2_Desc that will log the previous future resolved value.
 *
 * This function can be used to quickly inspect the value that an #Efl_Future2_Desc::cb
 * is returning. The returned value will be passed to the next future in the chain without
 * modifications.
 *
 * Example:
 *
 * @code
 *
 * efl_future2_chain(a_future, {.cb = cb1, .data = NULL},
 *                             //Inspect the cb1 value and pass to cb2 without modifications
 *                             efl_future2_cb_console("cb1 value:", NULL),
 *                             {.cb = cb2, .data = NULL},
 *                             //Inspect the cb2 value
 *                             efl_future2_cb_console("cb2 value:", NULL))
 * @endcode
 *
 * @param prefix A Prefix to print, if @c NULL an empty string ("") is used.
 * @param suffix A suffix to print. If @c NULL '\n' will be used. If suffix is provided
 * the '\n' must be provided by suffix otherwise the printed text will not contain
 * a line feed.
 * @return An #Efl_Future2_Desc
 * @see efl_future2_then()
 * @see #Efl_Future2_Desc
 * @see efl_future2_chain()
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 * @see efl_future2_then_from_desc()
 */
EAPI Efl_Future2_Desc efl_future2_cb_console(const char *prefix, const char *suffix);
/**
 * Creates an #Efl_Future2_Desc which will convert the the received eina value to a given type.
 *
 * @param type The Eina_Value_Type to convert to.
 * @return An #Efl_Future2_Desc
 * @see efl_future2_then()
 * @see #Efl_Future2_Desc
 * @see efl_future2_chain()
 * @see efl_future2_cb_easy_from_desc()
 * @see efl_future2_cb_easy()
 * @see efl_future2_then_from_desc()
 */
EAPI Efl_Future2_Desc efl_future2_cb_convert_to(const Eina_Value_Type *type);
/**
 * Gets the data attached to the promise.
 *
 * @return The data passed to efl_promise2_new() or @c NULL on error.
 * @see efl_promise2_new()
 * @see efl_promise2_resolve()
 * @see efl_promise2_reject()
 * @see efl_promise2_as_value()
 */
EAPI void *efl_promise2_data_get(const Efl_Promise2 *p) EINA_ARG_NONNULL(1);
/**
 * Creates an #Efl_Future2_Desc based on a #Efl_Future2_Cb_Easy_Desc
 *
 * This function aims to be used in conjuction with efl_future2_chain(),
 * efl_future2_then_from_desc() and friends and its main objective is to simplify
 * error handling and Eina_Value type checks.
 * It uses three callbacks to inform the user about the future's
 * result and life cycle. They are:
 *
 * @li #Efl_Future2_Cb_Easy_Desc::success: This callback is called when
 * the future execution was successful, this is, no errors occurred and
 * the result type matches #Efl_Future2_Cb_Easy_Desc::success_type. In case
 * #Efl_Future2_Cb_Easy_Desc::success_type is @c NULL, this function will
 * only be called if the future did not report an error. The value returned
 * by this function will be propagated to the next future in the chain (if any).
 *
 * @li #Efl_Future2_Cb_Easy_Desc::error: This callback is called when
 * the future result is an error or #Efl_Future2_Cb_Easy_Desc::success_type
 * does not match the future result type. The value returned
 * by this function will be propagated to the next future in the chain (if any).
 *
 * @li #Efl_Future2_Cb_Easy_Desc::free: Called after the future was freed and any resources
 * allocated must be freed at this point. This callback is always called.
 *
 * Check the example below for a sample usage:
 *
 * @code
 * static Eina_Value
 * _file_size_ok(void *data, Eina_Value v)
 * {
 *   Ctx *ctx = data;
 *   //Since an Efl_Future2_Cb_Easy_Desc::success_type was provided, there's no need to check the value type
 *   int s;
 *   eina_value_get(&v, &s);
 *   printf("File size is %d bytes\n", s);
 *   ctx->file_size = s;
 *   return v;
 * }
 *
 * static Eina_Value
 * _file_size_err(void *data, Eina_Error err)
 * {
 *   fprintf(stderr, "Could not read the file size. Reason: %s\n", eina_error_msg_get(err));
 *   //Stop propagating the error.
 *   return (Eina_Value){0};
 * }
 *
 * static void
 * _future_freed(void *data, const Efl_Future2 dead)
 * {
 *   Ctx *ctx = data;
 *   printf("Future %p deleted\n", dead);
 *   ctx->file_size_handler_cb(ctx->file_size);
 *   free(ctx);
 * }
 *
 * @code
 * void do_work(File_Size_Handler_Cb cb)
 * {
 *   Ctx *ctx = malloc(sizeof(Ctx));
 *   ctx->f = get_file_size("/tmp/todo.txt");
 *   ctx->file_size = -1;
 *   ctx->file_size_handler_cb = cb;
 *   efl_future2_then_from_desc(f, efl_future2_cb_easy(_file_size_ok, _file_size_err, _future_freed, EINA_VALUE_TYPE_INT, ctx));
 * }
 * @endcode
 *
 * @return An #Efl_Future2_Desc
 * @see efl_future2_chain()
 * @see efl_future2_then()
 * @see efl_future2_cb_easy()
 */
EAPI Efl_Future2_Desc efl_future2_cb_easy_from_desc(const Efl_Future2_Cb_Easy_Desc desc);
/**
 * Creates an all promise.
 *
 * Creates a promise that is resolved once all the futures
 * from the @p array are resolved.
 * The promise is resolved with an Eina_Value type array which
 * contains EINA_VALUE_TYPE_VALUE elements. The result array is
 * ordered according to the @p array argument. Example:
 *
 * @code
 *
 * static const char *
 * _get_operation_name_by_index(int idx)
 * {
 *   switch (idx)
 *   {
 *      case 0: return "Get file data";
 *      case 1: return "Get file size";
 *      default: return "sum";
 *   }
 * }
 *
 * static Eina_Value
 * _all_cb(const void *data EINA_UNUSED, const Eina_Value array, const Efl_Future2 *dead EINA_UNUSED)
 * {
 *    Eina_Error err;
 *    unsined int i, len;
 *
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *     {
 *       eina_value_get(&array, &err);
 *       fprintf(stderr, "Could not complete all operations. Reason: %s\n", eina_error_msg_get(err));
 *       return array;
 *     }
 *    len = eina_value_array_count(&array);
 *    for (i = 0; i < len; i++)
 *     {
 *       Eina_Value v;
 *       eina_value_array_get(&array, i, &v);
 *       if (v.type == EINA_VALUE_TYPE_ERROR)
 *        {
 *          eina_value_get(&array, &err);
 *          fprintf(stderr, "Could not complete operation '%s'. Reason: %s\n", _get_operation_name_by_index(i), eina_error_msg_get(err));
 *          continue;
 *        }
 *       if (!i)
 *        {
 *           const char *msg;
 *           if (v.type != EINA_VALUE_TYPE_STRING)
 *           {
 *             fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_STRING->name, v.type.name);
 *             continue;
 *           }
 *           eina_value_get(&v, &msg);
 *           printf("File content:%s\n", msg);
 *        }
 *       else if (i == 1)
 *        {
 *           int i;
 *           if (v.type != EINA_VALUE_TYPE_INT)
 *           {
 *             fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_INT->name, v.type.name);
 *             continue;
 *           }
 *           eina_value_get(&v, &i);
 *           printf("File size: %d\n", i);
 *        }
 *        else
 *        {
 *           double p;
 *           if (v.type != EINA_VALUE_TYPE_DOUBLE)
 *           {
 *             fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_DOUBLE->name, v.type.name);
 *             continue;
 *           }
 *           eina_value_get(&v, &p);
 *           printf("50 places of PI: %f\n", p);
 *        }
 *     }
 *    return array;
 * }
 *
 * void func(void)
 * {
 *   Efl_Future2 *f1, *f2, *f3, f_all;
 *
 *   f1 = read_file("/tmp/todo.txt");
 *   f2 = get_file_size("/tmp/file.txt");
 *   //calculates 50 places of PI
 *   f3 = calc_pi(50);
 *   f_all = efl_future2_all(f1, f2, f3);
 *   efl_future2_then(f_all, _all_cb, NULL);
 * }
 * @endcode
 *
 * @param array An array of futures, @c NULL terminated.
 * @return A promise or @c NULL on error.
 * @note On error all the futures will be CANCELED.
 * @see efl_future2_all_array()
 */
EAPI Efl_Promise2 *efl_promise2_all_array(Efl_Future2 *array[]);
/**
 * Creates a race promise.
 *
 * Creates a promise that resolves when a future from the @p array
 * is completed. The remaining futures will be canceled with the
 * error code `ECANCELED`
 *
 * The resulting value is an EINA_VALUE_TYPE_STRUCT with two fields:
 *
 * @li An field named "value" which contains an Eina_Value with the result itself.
 * @li A field named "index" which is an int that contains the index of the completed
 * function relative to the @p array;
 *
 * Example.
 *
 * @code
 *
 * static const char *
 * _get_operation_name_by_index(int idx)
 * {
 *   switch (idx)
 *   {
 *      case 0: return "Get file data";
 *      case 1: return "Get file size";
 *      default: return "sum";
 *   }
 * }
 *
 * static Eina_Value
 * _race_cb(const void *data EINA_UNUSED, const Eina_Value v)
 * {
 *    unsigned int i;
 *    Eina_Value result;
 *    Eina_Error err;
 *
 *     //No need to check for the 'v' type. efl_future2_cb_easy() did that for us.
 *     eina_value_struct_get(&v, "index", &i);
 *     //Get the operation result
 *     eina_value_struct_get(&v, "value", &result);
 *     if (!i)
 *      {
 *        const char *msg;
 *        if (result.type != EINA_VALUE_TYPE_STRING)
 *          fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_STRING->name, result.type.name);
 *        else
 *        {
 *          eina_value_get(&result, &msg);
 *          printf("File content:%s\n", msg);
 *        }
 *      }
 *     else if (i == 1)
 *       {
 *         int i;
 *         if (result.type != EINA_VALUE_TYPE_INT)
 *           fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_INT->name, v.type.name);
 *         else
 *         {
 *           eina_value_get(&result, &i);
 *           printf("File size: %d\n", i);
 *         }
 *       }
 *      else
 *       {
 *         double p;
 *         if (result.type != EINA_VALUE_TYPE_DOUBLE)
 *            fprintf(stderr, "Operation %s expects '%s' - received '%s'\n", _get_operation_name_by_index(i), EINA_VALUE_TYPE_DOUBLE->name, result.type.name);
 *         else
 *          {
 *            eina_value_get(&result, &p);
 *            printf("50 places of PI: %f\n", p);
 *          }
 *       }
 *     eina_value_flush(&result);
 *     return v;
 * }
 *
 * static Eina_Value
 * _race_err(void *data, Eina_Error err)
 * {
 *    fprintf(stderr, "Could not complete the race future. Reason: %s\n", eina_error_msg_get(err));
 *    return (Eina_Value ){0};
 * }
 *
 * void func(void)
 * {
 *   static const *Efl_Future2[] = {NULL, NULL, NULL, NULL};
 *   Eina_List *l = NULL;
 *
 *   futures[0] = read_file("/tmp/todo.txt");
 *   futures[1] = get_file_size("/tmp/file.txt");
 *   //calculates 50 places of PI
 *   futures[2] = calc_pi(50);
 *   f_race = efl_future2_race_array(futures);
 *   efl_future2_then(f_race, efl_future2_cb_easy(_race_cb, _race_err, NULL, EINA_VALUE_TYPE_STRUCT, NULL));
 * }
 * @endcode
 *
 * @param array An array of futures, @c NULL terminated.
 * @return A promise or @c NULL on error.
 * @note On error all the futures will be CANCELED.
 * @see efl_future2_race_array()
 */
EAPI Efl_Promise2 *efl_promise2_race_array(Efl_Future2 *array[]);

/**
 * Creates a future that will be resolved once all futures from @p array is resolved.
 * This is a helper over efl_promise2_all_array()
 * @see efl_promise2_all_array()
 */
static inline Efl_Future2 *
efl_future2_all_array(Efl_Future2 *array[])
{
   Efl_Promise2 *p = efl_promise2_all_array(array);
   if (!p) return NULL;
   return efl_future2_new(p);
}

/**
 * Creates a future that will be resolved once a future @p array is resolved.
 * This is a helper over efl_promise2_race_array()
 * @see efl_promise2_race_array()
 */
static inline Efl_Future2 *
efl_future2_race_array(Efl_Future2 *array[])
{
   Efl_Promise2 *p = efl_promise2_race_array(array);
   if (!p) return NULL;
   return efl_future2_new(p);
}

#ifndef __cplusplus
/**
 * A syntatic sugar over efl_promise2_race_array().
 * Usage:
 * @code
 * promise = efl_promise2_race(future1, future2, future3, future4);
 * @endcode
 * @see efl_promise2_race_array()
 */
#define efl_promise2_race(...) efl_promise2_race_array((Efl_Future2 *[]){__VA_ARGS__, NULL})
/**
 * A syntatic sugar over efl_future2_race_array().
 * Usage:
 * @code
 * future = efl_future2_race(future1, future2, future3, future4);
 * @endcode
 * @see efl_future2_racec_array()
 */
#define efl_future2_race(...) efl_future2_race_array((Efl_Future2 *[]){__VA_ARGS__, NULL})
/**
 * A syntatic sugar over efl_future2_all_array().
 * Usage:
 * @code
 * future = efl_future2_all(future1, future2, future3, future4);
 * @endcode
 * @see efl_future2_all_array()
 */
#define efl_future2_all(...) efl_future2_all_array((Efl_Future2 *[]){__VA_ARGS__, NULL})
/**
 * A syntatic sugar over efl_promise2_all_array().
 * Usage:
 * @code
 * promise = efl_promise2_all(future1, future2, future3, future4);
 * @endcode
 * @see efl_promise2_all_array()
 */
#define efl_promise2_all(...) efl_promise2_all_array((Efl_Future2 *[]){__VA_ARGS__, NULL})
/**
 * A syntatic sugar over efl_future2_cb_easy_from_desc().
 * Usage:
 * @code
 * future_desc = efl_future2_cb_easy(_success_cb, _error_cb, _free_cb, EINA_VALUE_TYPE_INT, my_data);
 * @endcode
 * @see efl_future2_cb_easy_from_desc()
 */
#define efl_future2_cb_easy(...) efl_future2_cb_easy_from_desc((Efl_Future2_Cb_Easy_Desc){__VA_ARGS__})
/**
 * A syntatic sugar over efl_future2_chain_array().
 * Usage:
 * @code
 * future = efl_future2_chain(future, {.cb = _my_cb, .data = my_data}, {.cb = _my_another_cb, .data = NULL});
 * @endcode
 * @see efl_future2_chain_array()
 */
#define efl_future2_chain(_prev, ...) efl_future2_chain_array(_prev, (Efl_Future2_Desc[]){__VA_ARGS__, {.cb = NULL, .data = NULL}})
/**
 * A syntatic sugar over efl_future2_then_from_desc().
 * Usage:
 * @code
 * future = efl_future2_then(future, _my_cb, my_data);
 * @endcode
 * @see efl_future2_then_from_desc()
 */
#define efl_future2_then(_prev, ...) efl_future2_then_from_desc(_prev, (Efl_Future2_Desc){__VA_ARGS__})
#endif

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
#endif

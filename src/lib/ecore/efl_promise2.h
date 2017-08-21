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
 * A callback used to inform that a future was resolved.
 * @param data The data provided by the user
 * @param value An Eina_Value which contains the operation result.
 * @param dead_future A pointer to the future that was completed.
 * @return An Eina_Value. If the callback does not generate a new Eina_Value you must
 * return the @p value argument
 * @note In some cases dead_future may be @c NULL which indicates that a future could
 * note be created and this function is only being called to let the user @c free @p data
 */
typedef Eina_Value (*Efl_Future2_Cb)(void *data, const Eina_Value value, const Efl_Future2 *dead_future);

/**
 * @typedef Efl_Promise2_Cancel_Cb Efl_Promise2_Cancel_Cb.
 * A callback used to inform that a promise was canceled.
 * @param data The data provided by the user.
 * @param dead_promise The canceled promise.
 */
typedef void (*Efl_Promise2_Cancel_Cb) (void *data, const Efl_Promise2 *dead_promise);

/**
 * @typedef Efl_Future2_Success_Cb Efl_Future2_Success_Cb.
 * A callback used to inform that the future completed with success.
 * @param data The data provided by the user.
 * @param value The operation result
 * @return An Eina_Value. If the callback does not generate a new Eina_Value you must
 * return the @p value argument
 */
typedef Eina_Value (*Efl_Future2_Success_Cb)(void *data, const Eina_Value value);

/**
 * @typedef Efl_Future2_Error_Cb Efl_Future2_Error_Cb.
 * A callback used to inform that the future completed with error.
 * @param data The data provided by the user.
 * @param error The operation error
 * @return An Eina_Value.
 */
typedef Eina_Value (*Efl_Future2_Error_Cb)(void *data, const Eina_Error error);

/**
 * @typedef Efl_Future2_Free_Cb Efl_Future2_Free_Cb.
 * A callback used to inform that the future will be freed and the user should also @c free the @p data.
 * @param data The data provided by the user.
 * @param dead_future The future that will be freed.
 */
typedef void (*Efl_Future2_Free_Cb)(void *data, const Efl_Future2 *dead_future);

/**
 * @struct _Efl_Future2_Cb_Easy_Desc
 *
 * A struct with callbacks to be used by efl_future2_cb_easy_from_desc()
 *
 * @see efl_future2_cb_easy_from_desc()
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
 * @see efl_future2_then()
 * @see efl_future2_chain_array()
 * @see efl_future2_cb_convert_to()
 * @see efl_future2_cb_console()
 * @see efl_future2_cb_easy_from_desc()
 */
struct _Efl_Future2_Desc {
   /**
    * Called when the future is resolved or rejected
    *
    * Use this function to free @p data if necessary.
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
 * using efl_future2_new(). Everytime a promise is created a Efl_Promise2_Cancel_Cb
 * must be provided which is used to free resources that was created.
 * Typically a promise is canceled when the future linked against it is canceled.
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
 *    //The _promise_cancel() will be used to clean ctx if the promise is canceled.
 *    ctx->p = efl_promise2_new(_promise_cancel, ctx);
 *    //A timer is scheduled in order to resolve the promise
 *    ctx->timer = ecore_timer_add(122, _promise_resolve, ctx);
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
 * @see efl_promise2_new()
 */
EAPI Efl_Promise2 *efl_promise2_new(Efl_Promise2_Cancel_Cb cancel_cb, const void *data);
/**
 * Resolves a promise.
 *
 *
 * This function schedules an resolve event into the mainloop, when
 * possible the whole future chain will be called.
 *
 * @param p A promise to resolve.
 * @param value The value to be delivered.
 * @note The @p value must not be flushed, this is not internally.
 */
EAPI void efl_promise2_resolve(Efl_Promise2 *p, Eina_Value value);
/**
 * Rejects a promise.
 *
 * This function schedules an reject event into the mainloop, when
 * possible the whole future chain will be called.
 *
 * @param p A promise to reject.
 * @param err An Eina_Error value
 * @note Internally this function will create an Eina_Value with type #EINA_VALUE_TYPE_ERROR.
 */
EAPI void efl_promise2_reject(Efl_Promise2 *p, Eina_Error err);
/**
 * Cancels a future.
 *
 * This function will cancel the whole future chain immediately (it will not be schedule to the next mainloop pass)
 * and it will also cancel the promise linked agains it. The Efl_Future2_Cb will be called
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
 * while chain will wait until the promise is resolved in
 * order to continue its execution. Example:
 *
 * @code
 * static Eina_Value
 * _file_data_ready(const *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
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
 *    }
 *
 *    eina_value_get(&v, &file_data);
 *    //count_words will count the words in the back ground and resolve the promise once it is over...
 *    p = count_words(strdup(file_data));
 *    return efl_promise2_as_value(p);
 * }
 *
 * static Eina_Value
 * _word_count_ready(const *data, const Eina_Value v, const Efl_Future2 *dead EINA_UNUSED)
 * {
 *    //The _word_count_ready will only be called once count_words() resolves/rejects the promise returned by _file_data_ready()
 *    int count;
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *    {
 *       Eina_Error err;
 *       eina_value_get(&v, &err);
 *       fprintf(stderr, "Could not count the file words. Reason: %s\n", eina_error_msg_get(err));
 *       ecore_main_loop_quit();
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
 * in this case the promise will also be canceled.
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
 * once it's resolve or rejected the callback will be informed.
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
 * }
 * @endcode
 *
 * @param prev A future to link against
 * @param desc A description struct contaning the callback and data.
 * @return A new future or @c NULL on error.
 * @note If an error happens the whole future chain will CANCELED and
 * desc.cb will be called in order to free desc.data.
 * @see efl_future2_new()
 */
EAPI Efl_Future2 *efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc);
/**
 * Creats a future chain.
 *
 * This function may be used to create a future chain, useful to perform operations
 * in a cascade order.
 *
 * @param prev The previous future
 * @param descs An array of descriptions
 * @return A future or @c NULL on error.
 * @note If an error happens the whole future chain will CANCELED and
 * desc.cb will be called in order to free desc.data.
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
 *                             //Inspect the cb1 value and pass to cb2 with not modifications
 *                             efl_future2_cb_console("cb1 value:", NULL),
 *                             {.cb = cb2, .data = NULL},
 *                             //Inspect the cb2 value
 *                             efl_future2_cb_console("cb2 value:", NULL))
 * @endcode
 *
 * @param prefix A Prefix to print. May be @c NULL.
 * @param suffix A suffix to print. If @c NULL '\n' will be used. If suffix is provided
 * the '\n' must be provided by suffix otherwise the printed text will not contain
 * a line feed.
 * @return An #Efl_Future2_Desc
 */
EAPI Efl_Future2_Desc efl_future2_cb_console(const char *prefix, const char *suffix);
/**
 * Creates an #Efl_Future2_Desc which will convert the the received eina value to a given type.
 *
 * @param type The Eina_Value_Type to converet to.
 * @return An #Efl_Future2_Desc
 */
EAPI Efl_Future2_Desc efl_future2_cb_convert_to(const Eina_Value_Type *type);
/**
 * Gets the data attached to the promise.
 *
 * @return The data passed to efl_promise2_new() or @c NULL on error.
 * @see efl_promise2_new()
 */
EAPI void *efl_promise2_data_get(const Efl_Promise2 *p) EINA_ARG_NONNULL(1);
/**
 * Creates an #Efl_Future2_Desc based on a #Efl_Future2_Cb_Easy_Desc
 *
 * This function aims to make it simpler to handle errors and value type checks.
 *
 * @return An #Efl_Future2_Desc
 */
EAPI Efl_Future2_Desc efl_future2_cb_easy_from_desc(const Efl_Future2_Cb_Easy_Desc desc);
/**
 * Creates an all promise.
 *
 * Creates a promise that is resolved once all the futures
 * from the @p array list are resolved.
 * The promise is resolved with an Eina_Value type array which
 * contains EINA_VALUE_TYPE_VALUE elements. The result array is
 * ordered according to the @p array argument. Example:
 *
 * @code
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
 *          const char *msg;
 *          if (!i) msg = "Get file data";
 *          else if (i == 1) msg = "Get file size";
 *          else msg = "sum";
 *          eina_value_get(&array, &err);
 *          fprintf(stderr, "Could not complete operation '%s'. Reason: %s\n", msg, eina_error_msg_get(err));
 *          continue;
 *        }
 *       if (!i)
 *        {
 *           const char *msg;
 *           eina_value_get(&v, &msg);
 *           printf("File content:%s\n", msg);
 *        }
 *       else
 *        {
 *           int i;
 *           eina_value_get(&v, &i);
 *           printf("%s: %d\n", i == 1 ? "File size" : "sum", i);
 *        }
 *     }
 *    return v;
 * }
 *
 * void func(void)
 * {
 *   Efl_Future2 *f1, *f2, *f3, f_all;
 *
 *   f1 = read_file("/tmp/todo.txt");
 *   f2 = get_file_size("/tmp/file.txt");
 *   f3 = sum(2 + 4);
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
 * Creates a promise that resolves when a future from the @p array list
 * is completed. The promise is resolved using the value reported
 * by the resolved future and all the remeaning futures are CANCELED!
 * Example.
 *
 * @code
 *
 * static Eina_Value
 * _race_cb(const void *data, const Eina_Value v, const Efl_Future2 *dead)
 * {
 *    size_t i;
 *    Eina_Error err;
 *    Efl_Futures **futures = data;
 *    if (v.type == EINA_VALUE_TYPE_ERROR)
 *     {
 *       eina_value_get(&v, &err);
 *       fprintf(stderr, "Error Reason: %s\n", eina_error_msg_get(err));
 *       return array;
 *     }
 *     for (i = 0; futures[i]; i++)
 *     {
 *       if (futures[i] != dead) continue;
 *       if (!i)
 *        {
 *           const char *msg;
 *           eina_value_get(&v, &msg);
 *           printf("File content:%s\n", msg);
 *        }
 *       else
 *        {
 *           int ivalue;
 *           eina_value_get(&v, &ivalue);
 *           printf("%s: %d\n", i == 1 ? "File size" : "sum", ivalue);
 *        }
 *        break;
 *     }
 *     return v;
 * }
 * void func(void)
 * {
 *   static const *Efl_Future2[] = {NULL, NULL, NULL, NULL};
 *   Eina_List *l = NULL;
 *
 *   futures[0] = read_file("/tmp/todo.txt");
 *   futures[1] = get_file_size("/tmp/file.txt");
 *   futures[2] = sum(2 + 4);
 *   f_race = efl_future2_race_array(futures);
 *   efl_future2_then(f_race, _race_cb, NULL);
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

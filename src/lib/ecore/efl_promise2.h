#ifndef _EFL_PROMISE2_H_
#define _EFL_PROMISE2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Eina.h>

typedef struct _Efl_Future2_Desc Efl_Future2_Desc;
typedef struct _Efl_Promise2 Efl_Promise2;
typedef struct _Efl_Future2 Efl_Future2;
typedef Eina_Value *(*Efl_Future2_Cb)(void *data, const Eina_Value *value);

struct _Efl_Future2_Desc {
   long api_version;
#define EFL_FUTURE2_DESC_API_VERSION (1)
   Efl_Future2_Cb success;
   Efl_Future2_Cb error;
   Eina_Free_Cb free;
   const void *data;
};

EAPI Efl_Promise2 *efl_promise2_new(Eina_Free_Cb cancel_cb, const void *data);
EAPI void efl_promise2_resolve(Efl_Promise2 *p, Eina_Value *value);
EAPI void efl_promise2_reject(Efl_Promise2 *p, Eina_Error err);
EAPI void efl_future2_cancel(Efl_Future2 *f);
EAPI Eina_Value *efl_promise2_as_value(Efl_Promise2 *p);
EAPI Eina_Value *efl_future2_as_value(Efl_Future2 *f);
EAPI Efl_Future2 *efl_future2_new(Efl_Promise2 *p);
EAPI Efl_Future2 *efl_future2_then_from_desc(Efl_Future2 *prev, const Efl_Future2_Desc desc);

#ifndef __cplusplus
#define efl_future2_then(p, ...) efl_future2_then_from_desc(p, (Efl_Future2_Desc){.api_version = EFL_FUTURE2_DESC_API_VERSION, __VA_ARGS__})
#endif


#ifdef __cplusplus
}
#endif
#endif

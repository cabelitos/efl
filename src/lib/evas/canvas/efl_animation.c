#include "evas_common_private.h"
#include "evas_private.h"
#include <Ecore.h>

#define MY_CLASS EFL_ANIMATION_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_CHECK_OR_RETURN(anim, ...) \
   do { \
      if (!anim) { \
         CRI("Efl_Animation " # anim " is NULL!"); \
         return __VA_ARGS__; \
      } \
      if (efl_animation_is_deleted(anim)) { \
         ERR("Efl_Animation " # anim " has already been deleted!"); \
         return __VA_ARGS__; \
      } \
   } while (0)

#define EFL_ANIMATION_DATA_GET(o, pd) \
   Evas_Object_Animation_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_CLASS)

struct _Evas_Object_Animation_Data
{
   /*
    * FIXME: The following properties should be added.
    * Current Interpolation Function
    * Eina_Bool auto_delete;
    * int repeat_time;
    * int current_loop;               //Default value is 1
    * Eina_Bool is_reverse;
    */
   Efl_Canvas_Object *target;

   double             duration;

   Eina_Bool          deleted : 1;
   Eina_Bool          state_keep : 1;
};

static void
_target_del_cb(void *data, const Efl_Event *event EINA_UNUSED)
{
   Eo *eo_obj = data;

   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj);
   EFL_ANIMATION_DATA_GET(eo_obj, pd);

   pd->target = NULL;
}

EOLIAN static void
_efl_animation_target_set(Eo *eo_obj,
                          Evas_Object_Animation_Data *pd,
                          Efl_Canvas_Object *target)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj);

   efl_event_callback_add(target, EFL_EVENT_DEL, _target_del_cb, eo_obj);

   pd->target = target;
}

EOLIAN static Efl_Canvas_Object *
_efl_animation_target_get(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, NULL);

   return pd->target;
}


EOLIAN static void
_efl_animation_duration_set(Eo *eo_obj,
                            Evas_Object_Animation_Data *pd,
                            double duration)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj);

   //If duration is 0, then display the end state of animation
   if (duration < 0.0) return;

   pd->duration = duration;
}

EOLIAN static double
_efl_animation_duration_get(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, 0.0);

   return pd->duration;
}

EOLIAN static Eina_Bool
_efl_animation_is_deleted(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(eo_obj, EINA_TRUE);

   return pd->deleted;
}

EOLIAN static void
_efl_animation_final_state_keep_set(Eo *eo_obj,
                                    Evas_Object_Animation_Data *pd,
                                    Eina_Bool state_keep)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj);

   if (pd->state_keep == state_keep) return;

   pd->state_keep = !!state_keep;
}

EOLIAN static Eina_Bool
_efl_animation_final_state_keep_get(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   EFL_ANIMATION_CHECK_OR_RETURN(eo_obj, EINA_FALSE);

   return pd->state_keep;
}

EOLIAN static Efl_Object *
_efl_animation_efl_object_constructor(Eo *eo_obj,
                                      Evas_Object_Animation_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->target = NULL;

   pd->duration = 0.0;

   pd->deleted = EINA_FALSE;
   pd->state_keep = EINA_FALSE;

   return eo_obj;
}

EOLIAN static void
_efl_animation_efl_object_destructor(Eo *eo_obj, Evas_Object_Animation_Data *pd)
{
   pd->deleted = EINA_TRUE;

   if (pd->target)
     efl_event_callback_del(pd->target, EFL_EVENT_DEL, _target_del_cb, eo_obj);

   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

/* Internal EO APIs */

EOAPI EFL_FUNC_BODY(efl_animation_is_deleted, Eina_Bool, 0);

#define EFL_ANIMATION_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_animation_is_deleted, _efl_animation_is_deleted)

#include "efl_animation.eo.c"

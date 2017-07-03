#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_INSTANCE_ALPHA_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_INSTANCE_ALPHA_CHECK_OR_RETURN(anim, ...) \
   do { \
      if (!anim) { \
         CRI("Efl_Animation_Instance " # anim " is NULL!"); \
         return __VA_ARGS__; \
      } \
      if (efl_animation_instance_is_deleted(anim)) { \
         ERR("Efl_Animation_Instance " # anim " has already been deleted!"); \
         return __VA_ARGS__; \
      } \
   } while (0)

#define EFL_ANIMATION_INSTANCE_ALPHA_DATA_GET(o, pd) \
   Evas_Object_Animation_Instance_Alpha_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_INSTANCE_ALPHA_CLASS)

typedef struct _Evas_Object_Animation_Instance_Alpha_Property
{
   double alpha;
} Evas_Object_Animation_Instance_Alpha_Property;

struct _Evas_Object_Animation_Instance_Alpha_Data
{
   Evas_Object_Animation_Instance_Alpha_Property from;
   Evas_Object_Animation_Instance_Alpha_Property to;
};

EOLIAN static void
_efl_animation_instance_alpha_alpha_set(Eo *eo_obj,
                                        Evas_Object_Animation_Instance_Alpha_Data *pd,
                                        double from_alpha,
                                        double to_alpha)
{
   EFL_ANIMATION_INSTANCE_ALPHA_CHECK_OR_RETURN(eo_obj);

   pd->from.alpha = from_alpha;
   pd->to.alpha = to_alpha;
}

EOLIAN static void
_efl_animation_instance_alpha_alpha_get(Eo *eo_obj EINA_UNUSED,
                                        Evas_Object_Animation_Instance_Alpha_Data *pd,
                                        double *from_alpha,
                                        double *to_alpha)
{
   EFL_ANIMATION_INSTANCE_ALPHA_CHECK_OR_RETURN(eo_obj);

   if (from_alpha)
     *from_alpha = pd->from.alpha;
   if (to_alpha)
     *to_alpha = pd->to.alpha;
}

static void
_progress_set(Eo *eo_obj, double progress)
{
   EFL_ANIMATION_INSTANCE_ALPHA_DATA_GET(eo_obj, pd);

   Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
   if (!target) return;

   double alpha
      = (pd->from.alpha * (1.0 - progress)) + (pd->to.alpha * progress);

   int color = (int)(alpha * 255);
   evas_object_color_set(target, color, color, color, color);
}

static void
_pre_animate_cb(void *data EINA_UNUSED, const Efl_Event *event)
{
   Efl_Animation_Instance_Animate_Event_Info *event_info = event->info;

   _progress_set(event->object, event_info->progress);
}

EOLIAN static Efl_Object *
_efl_animation_instance_alpha_efl_object_constructor(Eo *eo_obj,
                                                     Evas_Object_Animation_Instance_Alpha_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.alpha = 1.0;
   pd->to.alpha = 1.0;

   //pre animate event is supported within class only (protected event)
   efl_event_callback_add(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_PRE_ANIMATE,
                          _pre_animate_cb, NULL);

   return eo_obj;
}

EOLIAN static void
_efl_animation_instance_alpha_efl_object_destructor(Eo *eo_obj,
                                                    Evas_Object_Animation_Instance_Alpha_Data *pd EINA_UNUSED)
{
   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

EOLIAN static void
_efl_animation_instance_alpha_efl_animation_instance_final_state_show(Eo *eo_obj,
                                                                      Evas_Object_Animation_Instance_Alpha_Data *pd EINA_UNUSED)
{
   EFL_ANIMATION_INSTANCE_ALPHA_CHECK_OR_RETURN(eo_obj);

   _progress_set(eo_obj, 1.0);
}

/* Internal EO APIs */

EOAPI EFL_VOID_FUNC_BODYV(efl_animation_instance_alpha_set, EFL_FUNC_CALL(from_alpha, to_alpha), double from_alpha, double to_alpha);
EOAPI EFL_VOID_FUNC_BODYV_CONST(efl_animation_instance_alpha_get, EFL_FUNC_CALL(from_alpha, to_alpha), double *from_alpha, double *to_alpha);

#define EFL_ANIMATION_INSTANCE_ALPHA_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_alpha_set, _efl_animation_instance_alpha_alpha_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_alpha_get, _efl_animation_instance_alpha_alpha_get), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_final_state_show, _efl_animation_instance_alpha_efl_animation_instance_final_state_show)

#include "efl_animation_instance_alpha.eo.c"

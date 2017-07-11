#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_INSTANCE_TRANSLATE_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(inst, ...) \
   do { \
      if (!inst) { \
         CRI("Efl_Animation_Instance " # inst " is NULL!"); \
         return __VA_ARGS__; \
      } \
      if (efl_animation_instance_is_deleted(inst)) { \
         ERR("Efl_Animation_Instance " # inst " has already been deleted!"); \
         return __VA_ARGS__; \
      } \
   } while (0)

#define EFL_ANIMATION_INSTANCE_TRANSLATE_DATA_GET(o, pd) \
   Evas_Object_Animation_Instance_Translate_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_INSTANCE_TRANSLATE_CLASS)

typedef struct _Evas_Object_Animation_Instance_Translate_Property
{
   Evas_Coord move_x, move_y;
   Evas_Coord x, y;
} Evas_Object_Animation_Instance_Translate_Property;

struct _Evas_Object_Animation_Instance_Translate_Data
{
   Evas_Object_Animation_Instance_Translate_Property from;
   Evas_Object_Animation_Instance_Translate_Property to;

   Evas_Coord start_x, start_y;

   Eina_Bool use_rel_move;
};

EOLIAN static void
_efl_animation_instance_translate_translate_set(Eo *eo_obj,
                                                Evas_Object_Animation_Instance_Translate_Data *pd,
                                                Evas_Coord from_x,
                                                Evas_Coord from_y,
                                                Evas_Coord to_x,
                                                Evas_Coord to_y)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(eo_obj);

   pd->from.move_x = from_x;
   pd->from.move_y = from_y;

   pd->to.move_x = to_x;
   pd->to.move_y = to_y;

   //Update absolute coordinate based on relative move
   Evas_Coord x = 0;
   Evas_Coord y = 0;

   Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
   if (target)
     evas_object_geometry_get(target, &x, &y, NULL, NULL);

   pd->from.x = pd->from.move_x + x;
   pd->from.y = pd->from.move_y + y;

   pd->to.x = pd->to.move_x + x;
   pd->to.y = pd->to.move_y + y;

   pd->use_rel_move = EINA_TRUE;
}

EOLIAN static void
_efl_animation_instance_translate_translate_get(Eo *eo_obj,
                                                Evas_Object_Animation_Instance_Translate_Data *pd,
                                                Evas_Coord *from_x,
                                                Evas_Coord *from_y,
                                                Evas_Coord *to_x,
                                                Evas_Coord *to_y)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(eo_obj);

   //Update relative move based on absolute coordinate
   if (!pd->use_rel_move)
     {
        Evas_Coord x = 0;
        Evas_Coord y = 0;

        Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
        if (target)
          evas_object_geometry_get(target, &x, &y, NULL, NULL);

        pd->from.move_x = pd->from.x - x;
        pd->from.move_y = pd->from.y - y;

        pd->to.move_x = pd->to.x - x;
        pd->to.move_y = pd->to.y - y;
     }

   if (from_x)
     *from_x = pd->from.move_x;
   if (from_y)
     *from_y = pd->from.move_y;

   if (to_x)
     *to_x = pd->to.move_x;
   if (to_y)
     *to_y = pd->to.move_y;
}

EOLIAN static void
_efl_animation_instance_translate_translate_absolute_set(Eo *eo_obj,
                                                         Evas_Object_Animation_Instance_Translate_Data *pd,
                                                         Evas_Coord from_x,
                                                         Evas_Coord from_y,
                                                         Evas_Coord to_x,
                                                         Evas_Coord to_y)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(eo_obj);

   pd->from.x = from_x;
   pd->from.y = from_y;

   pd->to.x = to_x;
   pd->to.y = to_y;

   //Update relative move based on absolute coordinate
   Evas_Coord x = 0;
   Evas_Coord y = 0;

   Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
   if (target)
     evas_object_geometry_get(target, &x, &y, NULL, NULL);

   pd->from.move_x = pd->from.x - x;
   pd->from.move_y = pd->from.y - y;

   pd->to.move_x = pd->to.x - x;
   pd->to.move_y = pd->to.y - y;

   pd->use_rel_move = EINA_FALSE;
}

EOLIAN static void
_efl_animation_instance_translate_translate_absolute_get(Eo *eo_obj,
                                                         Evas_Object_Animation_Instance_Translate_Data *pd,
                                                         Evas_Coord *from_x,
                                                         Evas_Coord *from_y,
                                                         Evas_Coord *to_x,
                                                         Evas_Coord *to_y)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(eo_obj);

   //Update absolute coordinate based on relative move
   if (pd->use_rel_move)
     {
        Evas_Coord x = 0;
        Evas_Coord y = 0;

        Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
        if (target)
          evas_object_geometry_get(target, &x, &y, NULL, NULL);

        pd->from.x = pd->from.move_x + x;
        pd->from.y = pd->from.move_y + y;

        pd->to.x = pd->to.move_x + x;
        pd->to.y = pd->to.move_y + y;
     }

   if (from_x)
     *from_x = pd->from.x;
   if (from_y)
     *from_y = pd->from.y;

   if (to_x)
     *to_x = pd->to.x;
   if (to_y)
     *to_y = pd->to.y;
}

static void
_pre_start_cb(void *data EINA_UNUSED, const Efl_Event *event)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_DATA_GET(event->object, pd);

   pd->start_x = 0;
   pd->start_y = 0;

   Efl_Canvas_Object *target = efl_animation_instance_target_get(event->object);
   if (!target) return;

   Evas_Coord x, y;
   evas_object_geometry_get(target, &x, &y, NULL, NULL);

   pd->start_x = x;
   pd->start_y = y;
}

static void
_progress_set(Eo *eo_obj, double progress)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_DATA_GET(eo_obj, pd);

   Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
   if (!target) return;

   Evas_Coord new_x = 0;
   Evas_Coord new_y = 0;

   if (pd->use_rel_move)
     {
        Evas_Coord move_x, move_y;

        move_x
           = (pd->from.move_x * (1.0 - progress)) + (pd->to.move_x * progress);
        move_y
           = (pd->from.move_y * (1.0 - progress)) + (pd->to.move_y * progress);

        new_x = pd->start_x + move_x;
        new_y = pd->start_y + move_y;
     }
   else
     {
        new_x = (pd->from.x * (1.0 - progress)) + (pd->to.x * progress);
        new_y = (pd->from.y * (1.0 - progress)) + (pd->to.y * progress);
     }

   evas_object_move(target, new_x, new_y);
}

static void
_pre_animate_cb(void *data EINA_UNUSED, const Efl_Event *event)
{
   Efl_Animation_Instance_Animate_Event_Info *event_info = event->info;

   _progress_set(event->object, event_info->progress);
}

EOLIAN static Efl_Object *
_efl_animation_instance_translate_efl_object_constructor(Eo *eo_obj,
                                                         Evas_Object_Animation_Instance_Translate_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.move_x = 0;
   pd->from.move_y = 0;
   pd->from.x = 0;
   pd->from.y = 0;

   pd->to.move_x = 0;
   pd->to.move_y = 0;
   pd->to.x = 0;
   pd->to.y = 0;

   pd->start_x = 0;
   pd->start_y = 0;

   pd->use_rel_move = EINA_TRUE;

   //pre start event is supported within class only (protected event)
   efl_event_callback_add(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_PRE_START,
                          _pre_start_cb, NULL);

   //pre animate event is supported within class only (protected event)
   efl_event_callback_add(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_PRE_ANIMATE,
                          _pre_animate_cb, NULL);

   return eo_obj;
}

EOLIAN static void
_efl_animation_instance_translate_efl_object_destructor(Eo *eo_obj,
                                                        Evas_Object_Animation_Instance_Translate_Data *pd EINA_UNUSED)
{
   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

EOLIAN static void
_efl_animation_instance_translate_efl_animation_instance_final_state_show(Eo *eo_obj,
                                                                          Evas_Object_Animation_Instance_Translate_Data *pd EINA_UNUSED)
{
   EFL_ANIMATION_INSTANCE_TRANSLATE_CHECK_OR_RETURN(eo_obj);

   _progress_set(eo_obj, 1.0);
}

/* Internal EO APIs */

EOAPI EFL_VOID_FUNC_BODYV(efl_animation_instance_translate_set, EFL_FUNC_CALL(from_x, from_y, to_x, to_y), int from_x, int from_y, int to_x, int to_y);
EOAPI EFL_VOID_FUNC_BODYV_CONST(efl_animation_instance_translate_get, EFL_FUNC_CALL(from_x, from_y, to_x, to_y), int *from_x, int *from_y, int *to_x, int *to_y);

EOAPI EFL_VOID_FUNC_BODYV(efl_animation_instance_translate_absolute_set, EFL_FUNC_CALL(from_x, from_y, to_x, to_y), int from_x, int from_y, int to_x, int to_y);
EOAPI EFL_VOID_FUNC_BODYV_CONST(efl_animation_instance_translate_absolute_get, EFL_FUNC_CALL(from_x, from_y, to_x, to_y), int *from_x, int *from_y, int *to_x, int *to_y);

#define EFL_ANIMATION_INSTANCE_TRANSLATE_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_translate_set, _efl_animation_instance_translate_translate_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_translate_get, _efl_animation_instance_translate_translate_get), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_translate_absolute_set, _efl_animation_instance_translate_translate_absolute_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_translate_absolute_get, _efl_animation_instance_translate_translate_absolute_get), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_final_state_show, _efl_animation_instance_translate_efl_animation_instance_final_state_show)

#include "efl_animation_instance_translate.eo.c"

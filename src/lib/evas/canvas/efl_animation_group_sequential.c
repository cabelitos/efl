#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_GROUP_SEQUENTIAL_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_GROUP_SEQUENTIAL_CHECK_OR_RETURN(anim, ...) \
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

#define EFL_ANIMATION_GROUP_SEQUENTIAL_DATA_GET(o, pd) \
   Evas_Object_Animation_Group_Sequential_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_GROUP_SEQUENTIAL_CLASS)

struct _Evas_Object_Animation_Group_Sequential_Data
{
};

EOLIAN static Efl_Animation_Instance *
_efl_animation_group_sequential_efl_animation_instance_create(Eo *eo_obj,
                                                              Evas_Object_Animation_Group_Sequential_Data *pd EINA_UNUSED)
{
   EFL_ANIMATION_GROUP_SEQUENTIAL_CHECK_OR_RETURN(eo_obj, NULL);

   Efl_Animation_Instance_Group_Sequential *group_inst
      = efl_add(EFL_ANIMATION_INSTANCE_GROUP_SEQUENTIAL_CLASS, NULL);

   Eina_List *animations = efl_animation_group_animations_get(eo_obj);
   Eina_List *l;
   Efl_Animation *child_anim;
   Efl_Animation_Instance *child_inst;

   EINA_LIST_FOREACH(animations, l, child_anim)
     {
        child_inst = efl_animation_instance_create(child_anim);
        efl_animation_instance_group_instance_add(group_inst, child_inst);
     }

   Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
   if (target)
     efl_animation_instance_target_set(group_inst, target);

   double duration = efl_animation_duration_get(eo_obj);
   if (duration > 0.0)
     efl_animation_instance_duration_set(group_inst, duration);

   int repeat_count = efl_animation_repeat_count_get(eo_obj);
   efl_animation_instance_repeat_count_set(group_inst, repeat_count);

   Eina_Bool state_keep = efl_animation_final_state_keep_get(eo_obj);
   efl_animation_instance_final_state_keep_set(group_inst, state_keep);

   return group_inst;
}

#include "efl_animation_group_sequential.eo.c"

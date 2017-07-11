#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_INSTANCE_GROUP_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(inst, ...) \
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

#define EFL_ANIMATION_INSTANCE_GROUP_DATA_GET(o, pd) \
   Evas_Object_Animation_Instance_Group_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_INSTANCE_GROUP_CLASS)

struct _Evas_Object_Animation_Instance_Group_Data
{
   Eina_List *instances;
};

EOLIAN static void
_efl_animation_instance_group_instance_add(Eo *eo_obj,
                                           Evas_Object_Animation_Instance_Group_Data *pd,
                                           Efl_Animation_Instance *instance)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   if (!instance) return;

   Efl_Canvas_Object *target = efl_animation_instance_target_get(eo_obj);
   if (target)
     efl_animation_instance_target_set(instance, target);

   double duration = efl_animation_instance_duration_get(eo_obj);
   if (duration > 0.0)
     efl_animation_instance_duration_set(instance, duration);

   Eina_Bool state_keep = efl_animation_instance_final_state_keep_get(eo_obj);
   efl_animation_instance_final_state_keep_set(instance, state_keep);

   pd->instances = eina_list_append(pd->instances, instance);
}

EOLIAN static void
_efl_animation_instance_group_instance_del(Eo *eo_obj,
                                           Evas_Object_Animation_Instance_Group_Data *pd,
                                           Efl_Animation_Instance *instance)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   if (!instance) return;

   pd->instances = eina_list_remove(pd->instances, instance);
}

EOLIAN static Eina_List *
_efl_animation_instance_group_instances_get(Eo *eo_obj,
                                            Evas_Object_Animation_Instance_Group_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj, NULL);

   return pd->instances;
}

EOLIAN static void
_efl_animation_instance_group_efl_animation_instance_target_set(Eo *eo_obj,
                                                                Evas_Object_Animation_Instance_Group_Data *pd,
                                                                Efl_Canvas_Object *target)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(pd->instances, l, inst)
     {
        efl_animation_instance_target_set(inst, target);
     }

   efl_animation_instance_target_set(efl_super(eo_obj, MY_CLASS), target);
}

EOLIAN static void
_efl_animation_instance_group_efl_animation_instance_duration_set(Eo *eo_obj,
                                                                  Evas_Object_Animation_Instance_Group_Data *pd,
                                                                  double duration)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   if (duration <= 0.0) duration = 0.0;

   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(pd->instances, l, inst)
     {
        efl_animation_instance_duration_set(inst, duration);
     }

   efl_animation_instance_duration_set(efl_super(eo_obj, MY_CLASS), duration);
}

EOLIAN static void
_efl_animation_instance_group_efl_animation_instance_final_state_keep_set(Eo *eo_obj,
                                                                          Evas_Object_Animation_Instance_Group_Data *pd,
                                                                          Eina_Bool state_keep)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(pd->instances, l, inst)
     {
        efl_animation_instance_final_state_keep_set(inst, state_keep);
     }

   efl_animation_instance_final_state_keep_set(efl_super(eo_obj, MY_CLASS),
                                               state_keep);
}

EOLIAN static void
_efl_animation_instance_group_efl_animation_instance_map_reset(Eo *eo_obj,
                                                               Evas_Object_Animation_Instance_Group_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_CHECK_OR_RETURN(eo_obj);

   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(pd->instances, l, inst)
     {
        efl_animation_instance_map_reset(inst);
     }
}

EOLIAN static Efl_Object *
_efl_animation_instance_group_efl_object_constructor(Eo *eo_obj,
                                                     Evas_Object_Animation_Instance_Group_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->instances = NULL;

   return eo_obj;
}

EOLIAN static void
_efl_animation_instance_group_efl_object_destructor(Eo *eo_obj,
                                                    Evas_Object_Animation_Instance_Group_Data *pd)
{
   Efl_Animation_Instance *inst;

   EINA_LIST_FREE(pd->instances, inst)
     efl_del(inst);

   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

/* Internal EO APIs */

EOAPI EFL_VOID_FUNC_BODYV(efl_animation_instance_group_instance_add, EFL_FUNC_CALL(instance), Efl_Animation_Instance *instance);

EOAPI EFL_VOID_FUNC_BODYV(efl_animation_instance_group_instance_del, EFL_FUNC_CALL(instance), Efl_Animation_Instance *instance);

EOAPI EFL_FUNC_BODY(efl_animation_instance_group_instances_get, Eina_List *, NULL);

EWAPI const Efl_Event_Description _EFL_ANIMATION_INSTANCE_GROUP_EVENT_MEMBER_POST_END =
   EFL_EVENT_DESCRIPTION("member_post_end");

#define EFL_ANIMATION_INSTANCE_GROUP_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_group_instance_add, _efl_animation_instance_group_instance_add), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_group_instance_del, _efl_animation_instance_group_instance_del), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_group_instances_get, _efl_animation_instance_group_instances_get), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_target_set, _efl_animation_instance_group_efl_animation_instance_target_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_duration_set, _efl_animation_instance_group_efl_animation_instance_duration_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_final_state_keep_set, _efl_animation_instance_group_efl_animation_instance_final_state_keep_set), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_map_reset, _efl_animation_instance_group_efl_animation_instance_map_reset)

#include "efl_animation_instance_group.eo.c"

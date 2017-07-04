#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(anim, ...) \
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

#define EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(o, pd) \
   Evas_Object_Animation_Instance_Group_Parallel_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CLASS)

struct _Evas_Object_Animation_Instance_Group_Parallel_Data
{
   int        animate_inst_count;
   int        animate_inst_index;

   Eina_List *finished_inst_list;

   Eina_Bool  is_group_member : 1;
};

static void
_pre_animate_cb(void *data, const Efl_Event *event)
{
   Eo *eo_obj = data;

   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(eo_obj, pd);

   if (pd->animate_inst_index == 0)
     {
        //pre animate event is supported within class only (protected event)
        efl_event_callback_call(eo_obj,
                                EFL_ANIMATION_INSTANCE_EVENT_PRE_ANIMATE,
                                event->info);

        if (!pd->is_group_member)
          {
             Eina_List *insts
                = efl_animation_instance_group_instances_get(eo_obj);
             Eina_List *l;
             Efl_Animation_Instance *inst;

             //Reset previous map effect of all instances
             EINA_LIST_FOREACH(insts, l, inst)
               {
                  efl_animation_instance_map_reset(inst);
               }

             if (efl_animation_instance_final_state_keep_get(eo_obj))
               {
                  Eina_List *l;
                  Efl_Animation_Instance *inst;

                  EINA_LIST_FOREACH(pd->finished_inst_list, l, inst)
                    {
                       efl_animation_instance_final_state_show(inst);
                    }
               }
          }
     }

   pd->animate_inst_index++;

   if (pd->animate_inst_index >= pd->animate_inst_count)
     pd->animate_inst_index = 0;
}

static void
_post_end_cb(void *data, const Efl_Event *event)
{
   Eo *eo_obj = data;

   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(eo_obj, pd);

   //Save finished instances
   if (!eina_list_data_find(pd->finished_inst_list, event->object))
     {
        pd->finished_inst_list
           = eina_list_append(pd->finished_inst_list, event->object);
     }

   //member post end event is supported within class only (protected event)
   efl_event_callback_call(eo_obj,
                           EFL_ANIMATION_INSTANCE_GROUP_EVENT_MEMBER_POST_END,
                           NULL);

   pd->animate_inst_count--;

   if (pd->animate_inst_count == 0)
     {
        efl_event_callback_call(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_END, NULL);
        //post end event is supported within class only (protected event)
        efl_event_callback_call(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_POST_END,
                                NULL);
     }
}

static void
_member_post_end_cb(void *data, const Efl_Event *event)
{
   Eo *eo_obj = data;

   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(eo_obj, pd);

   //Save finished instances
   if (!eina_list_data_find(pd->finished_inst_list, event->object))
     {
        pd->finished_inst_list
           = eina_list_append(pd->finished_inst_list, event->object);
     }

   //member post end event is supported within class only (protected event)
   efl_event_callback_call(eo_obj,
                           EFL_ANIMATION_INSTANCE_GROUP_EVENT_MEMBER_POST_END,
                           NULL);
}

static Eina_Bool
_start(Eo *eo_obj, Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   pd->animate_inst_count = 0;
   pd->animate_inst_index = 0;

   efl_event_callback_call(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_START, NULL);

   Eina_Bool ret = EINA_FALSE;

   Eina_List *animations = efl_animation_instance_group_instances_get(eo_obj);
   Eina_List *l;
   Efl_Animation *anim;
   EINA_LIST_FOREACH(animations, l, anim)
     {
        //Data should be registered before animation starts
        if (efl_animation_instance_member_start(anim))
          {
             pd->animate_inst_count++;
             ret = EINA_TRUE;
          }
     }

   return ret;
}

EOLIAN static Eina_Bool
_efl_animation_instance_group_parallel_efl_animation_instance_start(Eo *eo_obj,
                                                                    Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj, EINA_FALSE);

   pd->is_group_member = EINA_FALSE;

   return _start(eo_obj, pd);
}

EOLIAN static Eina_Bool
_efl_animation_instance_group_parallel_efl_animation_instance_member_start(Eo *eo_obj,
                                                                           Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj, EINA_FALSE);

   pd->is_group_member = EINA_TRUE;

   return _start(eo_obj, pd);
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_final_state_show(Eo *eo_obj,
                                                                               Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   Eina_List *l;
   Efl_Animation *inst;
   EINA_LIST_FOREACH(pd->finished_inst_list, l, inst)
     {
        efl_animation_instance_final_state_show(inst);
     }
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_group_instance_add(Eo *eo_obj,
                                                                                 Evas_Object_Animation_Instance_Group_Parallel_Data *pd EINA_UNUSED,
                                                                                 Efl_Animation_Instance *instance)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (!instance) return;

   //pre animate event is supported within class only (protected event)
   efl_event_callback_add(instance, EFL_ANIMATION_INSTANCE_EVENT_PRE_ANIMATE,
                          _pre_animate_cb, eo_obj);

   //post end event is supported within class only (protected event)
   efl_event_callback_add(instance, EFL_ANIMATION_INSTANCE_EVENT_POST_END,
                          _post_end_cb, eo_obj);

   //member post end event is supported within class only (protected event)
   efl_event_callback_add(instance,
                          EFL_ANIMATION_INSTANCE_GROUP_EVENT_MEMBER_POST_END,
                          _member_post_end_cb, eo_obj);

   efl_animation_instance_group_instance_add(efl_super(eo_obj, MY_CLASS), instance);
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_group_instance_del(Eo *eo_obj,
                                                                                 Evas_Object_Animation_Instance_Group_Parallel_Data *pd EINA_UNUSED,
                                                                                 Efl_Animation_Instance *instance)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (!instance) return;

   efl_event_callback_del(instance, EFL_ANIMATION_INSTANCE_EVENT_PRE_ANIMATE,
                          _pre_animate_cb, eo_obj);

   efl_event_callback_del(instance, EFL_ANIMATION_INSTANCE_EVENT_POST_END,
                          _post_end_cb, eo_obj);

   efl_event_callback_del(instance,
                          EFL_ANIMATION_INSTANCE_GROUP_EVENT_MEMBER_POST_END,
                          _member_post_end_cb, eo_obj);

   efl_animation_instance_group_instance_del(efl_super(eo_obj, MY_CLASS), instance);
}

/* Internal EO APIs */

#define EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_member_start, _efl_animation_instance_group_parallel_efl_animation_instance_member_start), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_final_state_show, _efl_animation_instance_group_parallel_efl_animation_instance_final_state_show), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_group_instance_add, _efl_animation_instance_group_parallel_efl_animation_instance_group_instance_add), \
   EFL_OBJECT_OP_FUNC(efl_animation_instance_group_instance_del, _efl_animation_instance_group_parallel_efl_animation_instance_group_instance_del)

#include "efl_animation_instance_group_parallel.eo.c"

#include "evas_common_private.h"
#include "evas_private.h"
#include <Ecore.h>

#define MY_CLASS EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(inst, ...) \
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

#define EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(o, pd) \
   Evas_Object_Animation_Instance_Group_Parallel_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CLASS)

struct _Evas_Object_Animation_Instance_Group_Parallel_Data
{
   Ecore_Animator *start_animator;

   int        animate_inst_count;
   int        animate_inst_index;
   int        remaining_repeat_count;

   Eina_List *finished_inst_list;

   Eina_Bool  started : 1;
   Eina_Bool  paused : 1;
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

static Eina_Bool
_start_animator_cb(void *data)
{
   Eo *eo_obj = data;

   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_DATA_GET(eo_obj, pd);

   Eina_List *instances =
      efl_animation_instance_group_instances_get(eo_obj);
   Eina_List *l;
   Efl_Animation *inst;

   EINA_LIST_FOREACH(instances, l, inst)
     {
        //Data should be registered before animation instance starts
        efl_animation_instance_member_start(inst);
        pd->animate_inst_count++;
     }

   pd->start_animator = NULL;

   return ECORE_CALLBACK_CANCEL;
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
        int repeat_count = efl_animation_instance_repeat_count_get(eo_obj);
        if (repeat_count > 0)
          {
             pd->remaining_repeat_count--;
             if (pd->remaining_repeat_count >= 0)
               {
                  eina_list_free(pd->finished_inst_list);
                  pd->finished_inst_list = NULL;

                  pd->start_animator = ecore_animator_add(_start_animator_cb,
                                                          eo_obj);
                  return;
               }
          }
        else if (repeat_count == -1)
          {
             eina_list_free(pd->finished_inst_list);
             pd->finished_inst_list = NULL;

             pd->start_animator = ecore_animator_add(_start_animator_cb,
                                                     eo_obj);
             return;
          }

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

static void
_start(Eo *eo_obj, Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   ecore_animator_del(pd->start_animator);
   pd->start_animator = NULL;

   pd->animate_inst_count = 0;
   pd->animate_inst_index = 0;

   pd->started = EINA_TRUE;

   pd->remaining_repeat_count = efl_animation_instance_repeat_count_get(eo_obj);

   efl_event_callback_call(eo_obj, EFL_ANIMATION_INSTANCE_EVENT_START, NULL);

   Eina_List *instances = efl_animation_instance_group_instances_get(eo_obj);
   Eina_List *l;
   Efl_Animation *inst;
   EINA_LIST_FOREACH(instances, l, inst)
     {
        //Data should be registered before animation instance starts
        efl_animation_instance_member_start(inst);
        pd->animate_inst_count++;
     }
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_start(Eo *eo_obj,
                                                                    Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (pd->paused) return;

   pd->is_group_member = EINA_FALSE;

   _start(eo_obj, pd);
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_member_start(Eo *eo_obj,
                                                                           Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (pd->paused) return;

   pd->is_group_member = EINA_TRUE;

   _start(eo_obj, pd);
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_pause(Eo *eo_obj,
                                                                    Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (!pd->started) return;

   if (pd->paused) return;
   pd->paused = EINA_TRUE;

   Eina_List *inst_list = efl_animation_instance_group_instances_get(eo_obj);
   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(inst_list, l, inst)
     {
        efl_animation_instance_pause(inst);
     }
}

EOLIAN static void
_efl_animation_instance_group_parallel_efl_animation_instance_resume(Eo *eo_obj,
                                                                     Evas_Object_Animation_Instance_Group_Parallel_Data *pd)
{
   EFL_ANIMATION_INSTANCE_GROUP_PARALLEL_CHECK_OR_RETURN(eo_obj);

   if (!pd->started) return;

   if (!pd->paused) return;
   pd->paused = EINA_FALSE;

   Eina_List *inst_list = efl_animation_instance_group_instances_get(eo_obj);
   Eina_List *l;
   Efl_Animation_Instance *inst;

   EINA_LIST_FOREACH(inst_list, l, inst)
     {
        efl_animation_instance_resume(inst);
     }
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

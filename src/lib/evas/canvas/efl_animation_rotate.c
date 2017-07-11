#include "evas_common_private.h"
#include "evas_private.h"

#define MY_CLASS EFL_ANIMATION_ROTATE_CLASS
#define MY_CLASS_NAME efl_class_name_get(MY_CLASS)

#define EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(anim, ...) \
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

#define EFL_ANIMATION_ROTATE_DATA_GET(o, pd) \
   Evas_Object_Animation_Rotate_Data *pd = efl_data_scope_get(o, EFL_ANIMATION_ROTATE_CLASS)

typedef struct _Evas_Object_Animation_Rotate_Property
{
   double degree;
} Evas_Object_Animation_Rotate_Property;

typedef struct _Evas_Object_Animation_Rotate_Absolute_Pivot
{
   Evas_Coord cx, cy;
} Evas_Object_Animation_Rotate_Absolute_Pivot;

typedef struct _Evas_Object_Animation_Rotate_Relative_Pivot
{
   Efl_Canvas_Object *obj;
   double             cx, cy;
} Evas_Object_Animation_Rotate_Relative_Pivot;

struct _Evas_Object_Animation_Rotate_Data
{
   Evas_Object_Animation_Rotate_Property       from;
   Evas_Object_Animation_Rotate_Property       to;

   Evas_Object_Animation_Rotate_Absolute_Pivot abs_pivot;
   Evas_Object_Animation_Rotate_Relative_Pivot rel_pivot;

   Eina_Bool use_rel_pivot;
};

EOLIAN static void
_efl_animation_rotate_rotate_set(Eo *eo_obj,
                                 Evas_Object_Animation_Rotate_Data *pd,
                                 double from_degree,
                                 double to_degree,
                                 Efl_Canvas_Object *pivot,
                                 double cx,
                                 double cy)
{
   EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(eo_obj);

   pd->from.degree = from_degree;
   pd->to.degree = to_degree;

   pd->rel_pivot.obj = pivot;
   pd->rel_pivot.cx = cx;
   pd->rel_pivot.cy = cy;

   //Update absolute pivot based on relative pivot
   Evas_Coord x = 0;
   Evas_Coord y = 0;
   Evas_Coord w = 0;
   Evas_Coord h = 0;

   if (pivot)
     evas_object_geometry_get(pivot, &x, &y, &w, &h);
   else
     {
        Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
        if (target)
          evas_object_geometry_get(target, &x, &y, &w, &h);
     }

   pd->abs_pivot.cx = x + (w * cx);
   pd->abs_pivot.cy = y + (h * cy);

   pd->use_rel_pivot = EINA_TRUE;
}

EOLIAN static void
_efl_animation_rotate_rotate_get(Eo *eo_obj,
                                 Evas_Object_Animation_Rotate_Data *pd,
                                 double *from_degree,
                                 double *to_degree,
                                 Efl_Canvas_Object **pivot,
                                 double *cx,
                                 double *cy)
{
   EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(eo_obj);

   //Update relative pivot based on absolute pivot
   if (!pd->use_rel_pivot)
     {
        Evas_Coord x = 0;
        Evas_Coord y = 0;
        Evas_Coord w = 0;
        Evas_Coord h = 0;

        Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
        if (target)
          evas_object_geometry_get(target, &x, &y, &w, &h);

        if (w != 0)
          pd->rel_pivot.cx = (double)(pd->abs_pivot.cx - x) / w;
        else
          pd->rel_pivot.cx = 0.0;

        if (h != 0)
          pd->rel_pivot.cy = (double)(pd->abs_pivot.cy - y) / h;
        else
          pd->rel_pivot.cy = 0.0;
     }

   if (from_degree)
     *from_degree = pd->from.degree;

   if (to_degree)
     *to_degree = pd->to.degree;

   if (pivot)
     *pivot = pd->rel_pivot.obj;

   if (cx)
     *cx = pd->rel_pivot.cx;

   if (cy)
     *cy = pd->rel_pivot.cy;
}

EOLIAN static void
_efl_animation_rotate_rotate_absolute_set(Eo *eo_obj,
                                          Evas_Object_Animation_Rotate_Data *pd,
                                          double from_degree,
                                          double to_degree,
                                          Evas_Coord cx,
                                          Evas_Coord cy)
{
   EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(eo_obj);

   pd->from.degree = from_degree;
   pd->to.degree = to_degree;

   pd->abs_pivot.cx = cx;
   pd->abs_pivot.cy = cy;

   //Update relative pivot based on absolute pivot
   Evas_Coord x = 0;
   Evas_Coord y = 0;
   Evas_Coord w = 0;
   Evas_Coord h = 0;

   Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
   if (target)
     evas_object_geometry_get(target, &x, &y, &w, &h);

   pd->rel_pivot.obj = NULL;

   if (w != 0)
     pd->rel_pivot.cx = (double)(cx - x) / w;
   else
     pd->rel_pivot.cx = 0.0;

   if (h != 0)
     pd->rel_pivot.cy = (double)(cy - y) / h;
   else
     pd->rel_pivot.cy = 0.0;

   pd->use_rel_pivot = EINA_FALSE;
}

EOLIAN static void
_efl_animation_rotate_rotate_absolute_get(Eo *eo_obj,
                                          Evas_Object_Animation_Rotate_Data *pd,
                                          double *from_degree,
                                          double *to_degree,
                                          Evas_Coord *cx,
                                          Evas_Coord *cy)
{
   EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(eo_obj);

   //Update relative pivot based on absolute pivot
   if (pd->use_rel_pivot)
     {
        Evas_Coord x = 0;
        Evas_Coord y = 0;
        Evas_Coord w = 0;
        Evas_Coord h = 0;

        if (pd->rel_pivot.obj)
          evas_object_geometry_get(pd->rel_pivot.obj, &x, &y, &w, &h);
        else
          {
             Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
             if (target)
               evas_object_geometry_get(target, &x, &y, &w, &h);
          }

        pd->abs_pivot.cx = x + (w * pd->rel_pivot.cx);
        pd->abs_pivot.cy = y + (h * pd->rel_pivot.cy);
     }

   if (from_degree)
     *from_degree = pd->from.degree;

   if (to_degree)
     *to_degree = pd->to.degree;

   if (cx)
     *cx = pd->abs_pivot.cx;

   if (cy)
     *cy = pd->abs_pivot.cy;
}

EOLIAN static Efl_Animation_Instance *
_efl_animation_rotate_efl_animation_instance_create(Eo *eo_obj,
                                                    Evas_Object_Animation_Rotate_Data *pd)
{
   EFL_ANIMATION_ROTATE_CHECK_OR_RETURN(eo_obj, NULL);

   Efl_Animation_Instance_Rotate *instance
      = efl_add(EFL_ANIMATION_INSTANCE_ROTATE_CLASS, NULL);

   Efl_Canvas_Object *target = efl_animation_target_get(eo_obj);
   efl_animation_instance_target_set(instance, target);

   Eina_Bool state_keep = efl_animation_final_state_keep_get(eo_obj);
   efl_animation_instance_final_state_keep_set(instance, state_keep);

   double duration = efl_animation_duration_get(eo_obj);
   efl_animation_instance_duration_set(instance, duration);

   int repeat_count = efl_animation_repeat_count_get(eo_obj);
   efl_animation_instance_repeat_count_set(instance, repeat_count);

   if (pd->use_rel_pivot)
     {
        efl_animation_instance_rotate_set(instance,
                                          pd->from.degree,
                                          pd->to.degree,
                                          pd->rel_pivot.obj,
                                          pd->rel_pivot.cx,
                                          pd->rel_pivot.cy);
     }
   else
     {
        efl_animation_instance_rotate_absolute_set(instance,
                                                   pd->from.degree,
                                                   pd->to.degree,
                                                   pd->abs_pivot.cx,
                                                   pd->abs_pivot.cy);
     }

   return instance;
}

EOLIAN static Efl_Object *
_efl_animation_rotate_efl_object_constructor(Eo *eo_obj,
                                             Evas_Object_Animation_Rotate_Data *pd)
{
   eo_obj = efl_constructor(efl_super(eo_obj, MY_CLASS));

   pd->from.degree = 0.0;
   pd->to.degree = 0.0;

   pd->rel_pivot.obj = NULL;
   pd->rel_pivot.cx = 0.5;
   pd->rel_pivot.cy = 0.5;

   pd->abs_pivot.cx = 0;
   pd->abs_pivot.cy = 0;

   pd->use_rel_pivot = EINA_TRUE;

   return eo_obj;
}

EOLIAN static void
_efl_animation_rotate_efl_object_destructor(Eo *eo_obj,
                                            Evas_Object_Animation_Rotate_Data *pd EINA_UNUSED)
{
   efl_destructor(efl_super(eo_obj, MY_CLASS));
}

#include "efl_animation_rotate.eo.c"

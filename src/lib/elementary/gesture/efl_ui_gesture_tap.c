#include "efl_ui_gesture_private.h"

#define MY_CLASS EFL_UI_GESTURE_TAP_CLASS


EOLIAN static void
_efl_ui_gesture_tap_position_set(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Tap_Data *pd,
                                 int pos_x, int pos_y)
{
   pd->pos_x = pos_x;
   pd->pos_y = pos_y;
}

EOLIAN static void
_efl_ui_gesture_tap_position_get(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Tap_Data *pd,
                                 int *pos_x, int *pos_y)
{
   if (pos_x) *pos_x = pd->pos_x;
   if (pos_y) *pos_y = pd->pos_y;
}

EOLIAN static Efl_Object *
_efl_ui_gesture_tap_efl_object_constructor(Eo *obj, Efl_Ui_Gesture_Tap_Data *pd)
{
   Efl_Ui_Gesture_Data *gd;

   obj = efl_constructor(efl_super(obj, MY_CLASS));

   gd = efl_data_scope_get(obj, EFL_UI_GESTURE_CLASS);
   gd->type = EFL_UI_EVENT_GESTURE_TAP;

   return obj;
}

#include "efl_ui_gesture_tap.eo.c"
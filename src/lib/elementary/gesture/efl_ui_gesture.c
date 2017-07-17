#ifdef HAVE_CONFIG_H
#include "elementary_config.h"
#endif

#include "Elementary.h"
#include "efl_ui_gesture_private.h"

#define MY_CLASS EFL_UI_GESTURE_CLASS

EOLIAN static const Efl_Event_Description *
 _efl_ui_gesture_type_get(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Data *pd)
{
   return pd->type;
}

EOLIAN static Efl_Ui_Gesture_State
_efl_ui_gesture_state_get(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Data *pd)
{
   return pd->state;
}

EOLIAN static void
_efl_ui_gesture_hotspot_set(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Data *pd, int hotspot_x, int hotspot_y)
{
   pd->hotspot_x = hotspot_x;
   pd->hotspot_y = hotspot_y;
}


EOLIAN static void
_efl_ui_gesture_hotspot_get(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Data *pd, int *hotspot_x, int *hotspot_y)
{
   if (hotspot_x) *hotspot_x = pd->hotspot_x;
   if (hotspot_y) *hotspot_y = pd->hotspot_y;
}

#include "efl_ui_gesture.eo.c"
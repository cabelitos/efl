#include "efl_ui_gesture_private.h"

#define MY_CLASS EFL_UI_GESTURE_RECOGNIZER_CLASS

typedef struct _Efl_Ui_Gesture_Recognizer_Tap_Data
{

} Efl_Ui_Gesture_Recognizer_Tap_Data;

EOLIAN static Efl_Ui_Gesture *
_efl_ui_gesture_recognizer_tap_efl_ui_gesture_recognizer_create(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Recognizer_Tap_Data *pd EINA_UNUSED,
                                                                Efl_Object *target EINA_UNUSED)
{
   return efl_add(EFL_UI_GESTURE_TAP_CLASS, NULL);
}

EOLIAN static Efl_Ui_Gesture *
_efl_ui_gesture_recognizer_tap_efl_ui_gesture_recognizer_recognize(Eo *obj EINA_UNUSED,
                                                                   Efl_Ui_Gesture_Recognizer_Tap_Data *pd EINA_UNUSED,
                                                                   Efl_Ui_Gesture *gesture, Efl_Object *watched EINA_UNUSED,
                                                                   Efl_Input_Pointer *event EINA_UNUSED)
{
   return gesture;
}

EOLIAN static void
_efl_ui_gesture_recognizer_tap_efl_ui_gesture_recognizer_reset(Eo *obj,
                                                              Efl_Ui_Gesture_Recognizer_Tap_Data *pd EINA_UNUSED,
                                                              Efl_Ui_Gesture *gesture)
{
   Efl_Ui_Gesture_Tap_Data *tap;
   tap = efl_data_scope_get(gesture, EFL_UI_GESTURE_TAP_CLASS);
   tap->pos_x = 0;
   tap->pos_y = 0;
   efl_ui_gesture_recognizer_reset(efl_super(obj, MY_CLASS), gesture);
}

#include "efl_ui_gesture_recognizer_tap.eo.c"
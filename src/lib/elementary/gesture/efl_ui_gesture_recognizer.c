#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#include "Elementary.h"

#define MY_CLASS_NAME "Efl.Ui.Gesture.Recognizer"

typedef struct _Efl_Ui_Gesture_Recognizer_Data
{

} Efl_Ui_Gesture_Recognizer_Data;

EOLIAN static void
_efl_ui_gesture_recognizer_reset(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Recognizer_Data *pd EINA_UNUSED,
                                 Efl_Ui_Gesture *gesture EINA_UNUSED)
{

}

#include "efl_ui_gesture_recognizer.eo.c"



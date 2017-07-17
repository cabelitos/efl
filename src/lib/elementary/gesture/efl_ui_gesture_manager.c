#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#include "Elementary.h"

#define MY_CLASS_NAME "Efl.Ui.Gesture.Manager"

typedef struct _Efl_Ui_Gesture_Manager_Data
{

} Efl_Ui_Gesture_Manager_Data;

EOLIAN static const Efl_Event_Description *
_efl_ui_gesture_manager_register_recognizer(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Manager_Data *pd EINA_UNUSED,
                                            Efl_Ui_Gesture_Recognizer *recognizer EINA_UNUSED)
{
   return NULL;
}

EOLIAN static void
_efl_ui_gesture_manager_unregister_recognizer(Eo *obj EINA_UNUSED, Efl_Ui_Gesture_Manager_Data *pd EINA_UNUSED,
                                              const Efl_Event_Description *type EINA_UNUSED)
{

}

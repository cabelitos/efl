#ifndef EFL_UI_GESTURE_PRIVATE_H_
#define EFL_UI_GESTURE_PRIVATE_H_

#ifdef HAVE_CONFIG_H
#include "elementary_config.h"
#endif
#include "Elementary.h"

typedef struct _Efl_Ui_Gesture_Data             Efl_Ui_Gesture_Data;
typedef struct _Efl_Ui_Gesture_Tap_Data         Efl_Ui_Gesture_Tap_Data;

typedef struct _Efl_Ui_Gesture_Data
{
   const Efl_Event_Description    *type;
   Efl_Ui_Gesture_State      state;
   int                       hotspot_x;
   int                       hotspot_y;
} Efl_Ui_Gesture_Data;

typedef struct _Efl_Ui_Gesture_Tap_Data
{
   int      pos_x;
   int      pos_y;
} Efl_Ui_Gesture_Tap_Data;

#endif

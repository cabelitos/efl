#include "test.h"
#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif

#define EFL_PACK_LAYOUT_PROTECTED
#include <Elementary.h>
#include <assert.h>

#define CNT 10
static Eo *objects[CNT] = {};

typedef enum {
   NONE,
   NONE_BUT_FILL,
   EQUAL,
   ONE,
   TWO
} Weight_Mode;

static void
weights_cb(void *data, const Efl_Event *event)
{
   Weight_Mode mode = (Weight_Mode) efl_ui_radio_state_value_get(event->object);

   switch (mode)
     {
      case NONE:
        efl_gfx_size_hint_align_set(data, 0.5, 0.5);
        for (int i = 0; i < CNT; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        break;
      case NONE_BUT_FILL:
        efl_gfx_size_hint_align_set(data, -1, -1);
        for (int i = 0; i < CNT; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        break;
      case EQUAL:
        efl_gfx_size_hint_align_set(data, 0.5, 0.5);
        for (int i = 0; i < CNT; i++)
          efl_gfx_size_hint_weight_set(objects[i], 1, 1);
        break;
      case ONE:
        efl_gfx_size_hint_align_set(data, 0.5, 0.5);
        for (int i = 0; i < 6; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        efl_gfx_size_hint_weight_set(objects[6], 1, 1);
        for (int i = 7; i < CNT; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        break;
      case TWO:
        efl_gfx_size_hint_align_set(data, 0.5, 0.5);
        for (int i = 0; i < 5; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        efl_gfx_size_hint_weight_set(objects[5], 1, 1);
        efl_gfx_size_hint_weight_set(objects[6], 1, 1);
        for (int i = 7; i < CNT; i++)
          efl_gfx_size_hint_weight_set(objects[i], 0, 0);
        break;
      default: break;
     }
}

static void
user_min_slider_cb(void *data EINA_UNUSED, const Efl_Event *event)
{
   int val = efl_ui_range_value_get(event->object);

   efl_gfx_size_hint_min_set(objects[3], val, val);
}

static void
padding_slider_cb(void *data, const Efl_Event *event)
{
   int val = efl_ui_range_value_get(event->object);
   Eo *win = data, *box;

   box = efl_key_wref_get(win, "box");
   efl_pack_padding_set(box, val, val, EINA_TRUE);
}

static void
margin_slider_cb(void *data, const Efl_Event *event)
{
   int val = efl_ui_range_value_get(event->object);
   Eo *win = data, *box;

   box = efl_key_wref_get(win, "box");
   efl_gfx_size_hint_margin_set(box, val, val, val, val);
}

static void
alignh_slider_cb(void *data, const Efl_Event *event)
{
   double av, val;
   Eo *win = data, *box;

   box = efl_key_wref_get(win, "box");
   val = efl_ui_range_value_get(event->object);
   efl_pack_align_get(box, NULL, &av);
   efl_pack_align_set(box, val, av);
}

static void
alignv_slider_cb(void *data, const Efl_Event *event)
{
   double ah, val;
   Eo *win = data, *box;

   box = efl_key_wref_get(win, "box");
   val = efl_ui_range_value_get(event->object);
   efl_pack_align_get(box, &ah, NULL);
   efl_pack_align_set(box, ah, val);
}

static void
flow_check_cb(void *data, const Efl_Event *event)
{
   Eina_Bool chk = efl_ui_check_selected_get(event->object);
   Eina_List *list = NULL;
   Eina_Iterator *it;
   Eo *box, *win, *sobj, *parent;

   // Unpack all children from the box, delete it and repack into the new box

   win = data;
   box = efl_key_wref_get(win, "box");
   parent = efl_parent_get(box);
   it = efl_content_iterate(box);
   EINA_ITERATOR_FOREACH(it, sobj)
     list = eina_list_append(list, sobj);
   eina_iterator_free(it);
   efl_pack_unpack_all(box);
   efl_del(box);

   box = efl_add(chk ? EFL_UI_BOX_FLOW_CLASS : EFL_UI_BOX_CLASS, win);
   efl_content_set(parent, box);
   efl_key_wref_set(win, "box", box);

   EINA_LIST_FREE(list, sobj)
     efl_pack(box, sobj);
}

static void
horiz_check_cb(void *data, const Efl_Event *event)
{
   Eina_Bool chk = efl_ui_check_selected_get(event->object);
   Eo *box = efl_key_wref_get(data, "box");
   efl_orientation_set(box, chk ? EFL_ORIENT_HORIZONTAL : EFL_ORIENT_VERTICAL);
}

static void
homo_check_cb(void *data, const Efl_Event *event)
{
   Eina_Bool chk = efl_ui_check_selected_get(event->object);
   Eo *box = efl_key_wref_get(data, "box");
   efl_ui_box_flow_homogenous_set(box, chk);
}

static void
max_size_check_cb(void *data, const Efl_Event *event)
{
   Eina_Bool chk = efl_ui_check_selected_get(event->object);
   Eo *box = efl_key_wref_get(data, "box");
   efl_ui_box_flow_max_size_set(box, chk);
}

static void
_custom_layout_update(Eo *pack, const void *data EINA_UNUSED)
{
   Eina_Iterator *it = efl_content_iterate(pack);
   int count = efl_content_count(pack), i = 0;
   int px, py, pw, ph;
   Eo *sobj;

   // Note: This is a TERRIBLE layout. Just an example of the API, not showing
   // how to write a proper layout function.

   if (!count) return;

   efl_gfx_geometry_get(pack, &px, &py, &pw, &ph);
   EINA_ITERATOR_FOREACH(it, sobj)
     {
        int x, y, h, w, mw, mh;
        efl_gfx_size_hint_combined_min_get(sobj, &mw, &mh);
        x = (pw / count) * i;
        y = (ph / count) * i;
        w = mw;
        h = mh;
        efl_gfx_geometry_set(sobj, x + px, y + py, w, h);
        i++;
     }
   eina_iterator_free(it);
}

static void
custom_check_cb(void *data, const Efl_Event *event)
{
   EFL_OPS_DEFINE(custom_layout_ops,
                  EFL_OBJECT_OP_FUNC(efl_pack_layout_update, _custom_layout_update));

   Eina_Bool chk = efl_ui_check_selected_get(event->object);
   Eo *box, *win = data;

   box = efl_key_wref_get(win, "box");

   // Overriding just the one function we need
   efl_object_override(box, chk ? &custom_layout_ops : NULL);

   // Layout request is required as the pack object doesn't know the layout
   // function was just overridden.
   efl_pack_layout_request(box);
}

static inline Eo *
_radio_add(Eo *box, Eo *group, const char *text, Weight_Mode value)
{
   Eo *o, *win;

   //win = efl_provider_find(box, EFL_UI_WIN_CLASS);
   win = elm_object_top_widget_get(box); // FIXME: Needs EO
   o = efl_add(EFL_UI_RADIO_CLASS, box,
               efl_text_set(efl_added, text));
   efl_event_callback_add(o, EFL_UI_RADIO_EVENT_CHANGED, weights_cb, win);
   efl_gfx_size_hint_align_set(o, 0, 0.5);
   efl_ui_radio_state_value_set(o, value);
   efl_ui_radio_group_add(o, group);
   efl_pack(box, o);

   return o;
}

static inline Eo *
_check_add(Eo *box, const char *text, Efl_Event_Cb cb)
{
   Eo *o, *win;

   //win = efl_provider_find(box, EFL_UI_WIN_CLASS);
   win = elm_object_top_widget_get(box); // FIXME: Needs EO
   o = efl_add(EFL_UI_CHECK_CLASS, box,
               efl_text_set(efl_added, text));
   efl_ui_check_selected_set(o, 0); // why part of check class??
   efl_event_callback_add(o, EFL_UI_CHECK_EVENT_CHANGED, cb, win);
   efl_gfx_size_hint_align_set(o, 0, 0);
   efl_pack(box, o);

   return o;
}

static inline Eo *
_slider_add(Eo *box, const char *format, double value, double min, double max,
            Efl_Orient dir, Efl_Event_Cb cb)
{
   Eo *o, *win;

   //win = efl_provider_find(box, EFL_UI_WIN_CLASS);
   win = elm_object_top_widget_get(box); // FIXME: Needs EO
   o = efl_add(EFL_UI_SLIDER_CLASS, box,
               efl_ui_slider_indicator_format_set(efl_added, format),
               efl_ui_slider_indicator_show_set(efl_added, 1),
               efl_orientation_set(efl_added, dir),
               efl_gfx_size_hint_align_set(efl_added, 0.5, -1),
               efl_ui_range_min_max_set(efl_added, min, max),
               efl_ui_range_value_set(efl_added, value),
               efl_event_callback_add(efl_added, EFL_UI_SLIDER_EVENT_CHANGED, cb, win));
   efl_pack(box, o);

   return o;
}

void
test_ui_box(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Eo *win, *bx, *o, *vbox, *f, *hbox, *chk;
   int i = 0;

   win = efl_add(EFL_UI_WIN_CLASS, NULL,
                 efl_ui_win_name_set(efl_added, "ui-box"),
                 efl_text_set(efl_added, "Efl.Ui.Box"),
                 efl_ui_win_autodel_set(efl_added, EINA_TRUE));
   efl_gfx_size_set(win, 600, 400); // FIXME: doesn't work inside efl_add()

   vbox = efl_add(EFL_UI_BOX_CLASS, win,
                  efl_pack_padding_set(efl_added, 10, 10, EINA_TRUE),
                  efl_orientation_set(efl_added, EFL_ORIENT_DOWN),
                  efl_gfx_size_hint_margin_set(efl_added, 5, 5, 5, 5));
   efl_content_set(win, vbox);


   /* controls */
   f = efl_add(EFL_UI_FRAME_CLASS, win,
               efl_text_set(efl_added, "Controls"),
               efl_gfx_size_hint_align_set(efl_added, -1, -1),
               efl_gfx_size_hint_weight_set(efl_added, 1, 0));
   efl_pack(vbox, f);

   hbox = efl_add(EFL_UI_BOX_CLASS, win,
                  efl_pack_padding_set(efl_added, 10, 0, EINA_TRUE));
   efl_content_set(f, hbox);


   /* weights radio group */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
                efl_orientation_set(efl_added, EFL_ORIENT_DOWN),
                efl_pack_align_set(efl_added, 0.0, 0.0),
                efl_gfx_size_hint_align_set(efl_added, 0, -1),
                efl_gfx_size_hint_weight_set(efl_added, 0, 1));
   efl_pack(hbox, bx);

   chk = _radio_add(bx, NULL, "No weight", NONE);
   _radio_add(bx, chk, "No weight + box fill", NONE_BUT_FILL);
   _radio_add(bx, chk, "Equal weights", EQUAL);
   _radio_add(bx, chk, "One weight only", ONE);
   _radio_add(bx, chk, "Two weights", TWO);


   /* misc */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
                efl_orientation_set(efl_added, EFL_ORIENT_DOWN),
                efl_gfx_size_hint_align_set(efl_added, 0, -1),
                efl_gfx_size_hint_weight_set(efl_added, 0, 1));
   efl_pack(hbox, bx);

   o = efl_add(EFL_UI_TEXT_CLASS, win, efl_text_set(efl_added, "Misc"));
   efl_pack(bx, o);

   _check_add(bx, "Flow", flow_check_cb);
   o = _check_add(bx, "Horizontal", horiz_check_cb);
   efl_ui_check_selected_set(o, 1);
   _check_add(bx, "Homogenous", homo_check_cb);
   _check_add(bx, "Homogenous + Max", max_size_check_cb);
   _check_add(bx, "Custom layout", custom_check_cb);


   /* user min size setter */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
                efl_orientation_set(efl_added, EFL_ORIENT_DOWN),
                efl_gfx_size_hint_align_set(efl_added, 0, -1),
                efl_gfx_size_hint_weight_set(efl_added, 0, 1));
   efl_pack(hbox, bx);

   o = efl_add(EFL_UI_TEXT_CLASS, win, efl_text_set(efl_added, "User min size"),
               efl_gfx_size_hint_weight_set(efl_added, 1, 0));
   efl_pack(bx, o);

   o = efl_add(EFL_UI_SLIDER_CLASS, win,
               efl_ui_slider_indicator_format_set(efl_added, "%.0fpx"),
               efl_ui_slider_indicator_show_set(efl_added, 1),
               efl_orientation_set(efl_added, EFL_ORIENT_UP),
               efl_gfx_size_hint_align_set(efl_added, 0.5, -1),
               efl_ui_range_min_max_set(efl_added, 0, 250),
               efl_ui_range_value_set(efl_added, 10));
   efl_event_callback_add(o, EFL_UI_SLIDER_EVENT_CHANGED, user_min_slider_cb, win);
   efl_pack(bx, o);


   /* inner box padding */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
                efl_orientation_set(efl_added, EFL_ORIENT_DOWN));
   efl_gfx_size_hint_align_set(bx, 0, -1);
   efl_gfx_size_hint_weight_set(bx, 0, 1);
   efl_pack(hbox, bx);

   o = efl_add(EFL_UI_TEXT_CLASS, win, efl_text_set(efl_added, "Padding"),
               efl_gfx_size_hint_weight_set(efl_added, 1, 0));
   efl_pack(bx, o);

   _slider_add(bx, "%.0fpx", 10, 0, 40, EFL_ORIENT_UP, padding_slider_cb);


   /* outer margin */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
               efl_orientation_set(efl_added, EFL_ORIENT_DOWN));
   efl_gfx_size_hint_align_set(bx, 0, -1);
   efl_gfx_size_hint_weight_set(bx, 0, 1);
   efl_pack(hbox, bx);

   o = efl_add(EFL_UI_TEXT_CLASS, win, efl_text_set(efl_added, "Margin"),
               efl_gfx_size_hint_weight_set(efl_added, 1, 0));
   efl_pack(bx, o);

   _slider_add(bx, "%.0fpx", 10, 0, 40, EFL_ORIENT_UP, margin_slider_cb);


   /* Box align */
   bx = efl_add(EFL_UI_BOX_CLASS, win,
               efl_orientation_set(efl_added, EFL_ORIENT_DOWN));
   efl_gfx_size_hint_align_set(bx, 0, -1);
   efl_gfx_size_hint_weight_set(bx, 1, 1);
   efl_pack(hbox, bx);

   o = efl_add(EFL_UI_TEXT_CLASS, win, efl_text_set(efl_added, "Box align"),
               efl_gfx_size_hint_weight_set(efl_added, 1, 0));
   efl_pack(bx, o);

   o = _slider_add(bx, "%.1f", 0.5, -0.1, 1.0, EFL_ORIENT_DOWN, alignv_slider_cb);
   efl_ui_slider_step_set(o, 0.1); // not range api?

   o = _slider_add(bx, "%.1f", 0.5, -0.1, 1.0, EFL_ORIENT_RIGHT, alignh_slider_cb);
   efl_gfx_size_hint_weight_set(o, 1, 0);
   efl_gfx_size_hint_align_set(o, -1, 0.5);
   efl_gfx_size_hint_min_set(o, 100, 0);
   efl_ui_slider_step_set(o, 0.1); // not range api?


   /* contents */
   f = efl_add(EFL_UI_FRAME_CLASS, win,
               efl_text_set(efl_added, "Contents"),
               efl_gfx_size_hint_align_set(efl_added, -1, -1));
   efl_pack(vbox, f);

   bx = efl_add(EFL_UI_BOX_CLASS, win);
   efl_key_wref_set(win, "box", bx);
   efl_pack_padding_set(bx, 10, 10, EINA_TRUE);
   efl_gfx_size_hint_align_set(bx, 0.5, 0.5);
   efl_content_set(f, bx);

   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "Default"));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "Fill H+V"),
                              efl_gfx_size_hint_align_set(efl_added, -1, -1));
   objects[i++] = o = efl_add(EFL_UI_TEXT_CLASS, win,
                              efl_text_wrap_set(efl_added, EFL_TEXT_FORMAT_WRAP_WORD), // FIXME inconsistent names
                              efl_text_set(efl_added, "This label is not marked as fill"));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "Min size"),
                              efl_gfx_size_hint_align_set(efl_added, 0.5, 1.0));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "Align top"),
                              efl_gfx_size_hint_align_set(efl_added, 0.5, 0.0));
   objects[i++] = o = efl_add(EFL_UI_TEXT_CLASS, win,
                              efl_text_wrap_set(efl_added, EFL_TEXT_FORMAT_WRAP_WORD), // FIXME inconsistent names
                              efl_text_set(efl_added, "This label on the other hand\n"
                                                      "is marked as align=fill."),
                              efl_gfx_size_hint_align_set(efl_added, -1, -1));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "Button with a max size of 200x100."),
                              efl_gfx_size_hint_max_set(efl_added, 200, 100),
                              efl_gfx_size_hint_align_set(efl_added, -1, -1));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "BtnA"));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "BtnB"));
   objects[i++] = o = efl_add(EFL_UI_BUTTON_CLASS, win,
                              efl_text_set(efl_added, "BtnC"));

   assert(i == CNT);
   for (i = 0; i < CNT; i++)
       {
          efl_gfx_size_hint_weight_set(o, 0, 0);
          efl_pack(bx, objects[i]);
       }

   efl_gfx_visible_set(win, 1);
}

#include "type_lookup.hh"

namespace eolian_cxx {

using efl::eolian::eolian_type;

// Keep the table sorted!
// This can help: cat type_lookup_table | LC_ALL=C sort
const lookup_table_type
type_lookup_table
{
  {"Ecore_Cb", eolian_type::callback_, {"Ecore.h"}},
  {"Ecore_Task_Cb", eolian_type::callback_, {"Ecore.h"}},
  {"Ecore_Timeline_Cb", eolian_type::callback_, {"Ecore.h"}},
  {"Edje_Signal_Cb", eolian_type::callback_, {"Edje.h"}},
  {"Eina_Accessor *", eolian_type::complex_, false, false, "efl::eina::accessor", {"eina-cxx/eina_accessor.hh"}},
  {"Eina_Bool", eolian_type::simple_, false, false, "bool", {}},
  {"Eina_Bool *", eolian_type::simple_, false, false, "bool*", {}},
  {"Eina_Inlist *", eolian_type::complex_, false, false, "efl::eina::range_inlist", {"eina-cxx/eina_inlist.hh"}},
  {"Eina_Inlist *", eolian_type::complex_, false, true, "efl::eina::inlist", {"eina-cxx/eina_inlist.hh"}},
  {"Eina_Iterator *", eolian_type::complex_, false, false, "efl::eina::iterator", {"eina-cxx/eina_iterator.hh"}},
  {"Eina_List *", eolian_type::complex_, false, false, "efl::eina::range_list", {"eina-cxx/eina_list.hh"}},
  {"Eina_List *", eolian_type::complex_, false, true, "efl::eina::list", {"eina-cxx/eina_list.hh"}},
  {"const Eina_List *", eolian_type::complex_, true, false, "efl::eina::crange_list", {"eina-cxx/eina_list.hh"}},
  {"Eio_Filter_Direct_Cb", eolian_type::callback_, {"Eio.h"}},
  {"Emodel *", eolian_type::simple_, false, false, "emodel", {"Emodel.hh"}},
  {"Eo *", eolian_type::simple_, false, true, "efl::eo::base", {"eo_base.hh"}},
  {"Eo *", eolian_type::simple_, false, false, "efl::eo::base", {"eo_base.hh"}},
  //{"Evas_Object_Box_Layout", eolian_type::callback_, {"Evas.h"}},
  {"Evas_Object *", eolian_type::simple_, false, false, "evas::object", {"canvas/evas_object.eo.hh"}},
  {"char *", eolian_type::simple_, false, true, "std::unique_ptr<char*>", {"memory"}},
  {"const Eina_Inlist *", eolian_type::complex_, false, false, "efl::eina::range_inlist", {"eina-cxx/eina_inlist.hh"}},
  {"const Eina_List *", eolian_type::complex_, false, false, "efl::eina::range_list", {"eina-cxx/eina_ptrlist.hh"}},
  {"const char *", eolian_type::simple_, false, false, "std::string", {"string"}},
};

}

#ifndef EOLIAN_MONO_CLASS_DEFINITION_HPP
#define EOLIAN_MONO_CLASS_DEFINITION_HPP

#include "grammar/integral.hpp"
#include "grammar/generator.hpp"
#include "grammar/klass_def.hpp"
#include "function_blacklist.hh"

#include "grammar/indentation.hpp"
#include "grammar/list.hpp"
#include "grammar/alternative.hpp"
#include "type.hh"
#include "namespace.hh"
#include "function_definition.hh"
#include "function_registration.hh"
#include "function_declaration.hh"
#include "grammar/string.hpp"
#include "grammar/attribute_replace.hpp"
#include "grammar/integral.hpp"
#include "grammar/case.hpp"
#include "using_decl.hh"

#include <string>
#include <algorithm>

namespace eolian_mono {

struct klass
{
   template <typename OutputIterator, typename Context>
   bool generate(OutputIterator sink, attributes::klass_def const& cls, Context const& context) const
   {
     std::string suffix, class_type;
     switch(cls.type)
       {
       case attributes::class_type::regular:
       case attributes::class_type::abstract_:
         class_type = "class";
         suffix = "CLASS";
         break;
       case attributes::class_type::mixin:
         class_type = "interface";
         suffix = "MIXIN";
         break;
       case attributes::class_type::interface_:
         class_type = "interface";
         suffix = "INTERFACE";
         break;
       }

     std::vector<std::string> namespaces = escape_namespace(cls.namespaces);
     auto open_namespace = *("namespace " << string << " { ") << "\n";
     if(!as_generator(open_namespace).generate(sink, namespaces, add_lower_case_context(context))) return false;

     // Interface class
     if(!as_generator
        (
         "public " /*<< class_type*/ "interface" /*<<*/ " " << string << " : "
         )
        .generate(sink, cls.cxx_name, context))
       return false;
     for(auto first = std::begin(cls.immediate_inherits)
           , last = std::end(cls.immediate_inherits); first != last; ++first)
       {
         if(!as_generator("\n" << scope_tab << *(lower_case[string] << ".") << string << " ,")
            .generate(sink, std::make_tuple(escape_namespace(first->namespaces), first->eolian_name), context))
           return false;
         // if(std::next(first) != last)
         //   *sink++ = ',';
       }
     // if(cls.immediate_inherits.empty())
       if(!as_generator("\n" << scope_tab << "efl.eo.IWrapper").generate(sink, attributes::unused, context)) return false;
     if(!as_generator("\n{\n").generate(sink, attributes::unused, context)) return false;
     
     if(!as_generator(*(scope_tab << function_declaration))
        .generate(sink, cls.functions, context)) return false;

     // FIXME Move the event generator into another generator like function?
     for (auto &&e : cls.events)
       {
         //FIXME Add a way to generate camelcase names
         if (!as_generator(
                     scope_tab << "event EventHandler "
                     << grammar::string_replace(',', '_') << ";\n"
                     ).generate(sink, e.name, add_upper_case_context(context)))
             return false;
       }

     if(!as_generator("}\n").generate(sink, attributes::unused, context)) return false;

     auto class_get_name = *(lower_case[string] << "_") << lower_case[string] << "_class_get";
     // Concrete class
     // if(class_type == "class")
     //   {
         if(!as_generator
            (
             "public class " << string << "Concrete : " << string << "\n{\n"
             << scope_tab << "System.IntPtr handle;\n"
             << scope_tab << "Dictionary<string, int> event_cb_count = new Dictionary<string, int>();\n"
             << scope_tab << "public System.IntPtr raw_handle {\n"
             << scope_tab << scope_tab << "get { return handle; }\n"
             << scope_tab << "}\n"
             << scope_tab << "public System.IntPtr raw_klass {\n"
             << scope_tab << scope_tab << "get { return efl.eo.Globals.efl_class_get(handle); }\n"
             << scope_tab << "}\n"
             << scope_tab << "[System.Runtime.InteropServices.DllImport(\"" << context_find_tag<library_context>(context).library_name
             << "\")] static extern System.IntPtr\n"
             << scope_tab << scope_tab << class_get_name << "();\n"
             << (class_type == "class" ? "" : "/*")
             << scope_tab << "public " << string << "Concrete(efl.Object parent = null)\n"
             << scope_tab << "{\n"
             << scope_tab << scope_tab << "System.IntPtr klass = " << class_get_name << "();\n"
             << scope_tab << scope_tab << "System.IntPtr parent_ptr = System.IntPtr.Zero;\n"
             << scope_tab << scope_tab << "if(parent != null)\n"
             << scope_tab << scope_tab << scope_tab << "parent_ptr = parent.raw_handle;\n"
             << scope_tab << scope_tab << "System.IntPtr eo = efl.eo.Globals._efl_add_internal_start(\"file\", 0, klass, parent_ptr, 0, 0);\n"
             << scope_tab << scope_tab << "handle = efl.eo.Globals._efl_add_end(eo, 0, 0);\n"
             << scope_tab << scope_tab << "register_event_proxies();\n"
             << scope_tab << "}\n"
             << (class_type == "class" ? "" : "*/")
             << scope_tab << "public " << string << "Concrete(System.IntPtr raw)\n"
             << scope_tab << "{\n"
             << scope_tab << scope_tab << "handle = raw;\n"
             << scope_tab << "}\n"
             /* << scope_tab << "public delegate void EflEventHandler(object sender, EventArgs e);\n" */
            )
            .generate(sink, std::make_tuple(cls.cxx_name, cls.cxx_name, cls.namespaces, cls.eolian_name, cls.cxx_name, cls.namespaces, cls.eolian_name, cls.cxx_name), context))
           return false;

         if (!generate_events(sink, cls, context))
             return false;

         if (!generate_events_registration(sink, cls, context))
             return false;
     
         if(!as_generator(*(function_definition))
            .generate(sink, cls.functions, context)) return false;

         for(auto first = std::begin(cls.inherits)
               , last = std::end(cls.inherits); first != last; ++first)
           {
             attributes::klass_def klass(get_klass(*first));
             
             if(!as_generator(*(function_definition))
                .generate(sink, klass.functions, context)) return false;
           }

         
         if(!as_generator("}\n").generate(sink, attributes::unused, context)) return false;
       // }

     // Inherit class
     if(class_type == "class")
       {
         if(!as_generator
            (
             "public " << class_type << " " << string << "Inherit : " << string << "\n{\n"
             << scope_tab << "System.IntPtr handle;\n"
             << scope_tab << "Dictionary<string, int> event_cb_count = new Dictionary<string, int>();\n"
             << scope_tab << "public static System.IntPtr klass;\n"
             << scope_tab << "public System.IntPtr raw_handle {\n"
             << scope_tab << scope_tab << "get { return handle; }\n"
             << scope_tab << "}\n"
             << scope_tab << "public System.IntPtr raw_klass {\n"
             << scope_tab << scope_tab << "get { return klass; }\n"
             << scope_tab << "}\n"
             << scope_tab << "[System.Runtime.InteropServices.DllImport(\"" << context_find_tag<library_context>(context).library_name
             << "\")] static extern System.IntPtr\n"
             << scope_tab << scope_tab << class_get_name << "();\n"
             << scope_tab << "public " << string << "Inherit(efl.Object parent = null, System.Type interface1 = null)\n"
             << scope_tab << "{\n"
             << scope_tab << scope_tab << "klass = efl.eo.Globals.register_class(new efl.eo.Globals.class_initializer(" << string << "NativeInherit.class_initializer), " << class_get_name << "());\n"
             //<< scope_tab << scope_tab << "klass = efl.eo.Globals.register_class(null/*new efl.eo.Globals.class_initializer(" << string << "NativeInherit.class_initializer)*/, " << class_get_name << "());\n"             
             << scope_tab << scope_tab << "handle = efl.eo.Globals.instantiate(klass, parent);\n"
             << scope_tab << scope_tab << "efl.eo.Globals.data_set(this);\n"
             << scope_tab << "}\n"
            )
            .generate(sink, std::make_tuple(cls.cxx_name, cls.cxx_name, cls.namespaces, cls.eolian_name, cls.cxx_name, cls.cxx_name, cls.namespaces, cls.eolian_name, cls.cxx_name), context))
           return false;

         if (!generate_events(sink, cls, context))
             return false;
     
         if(!as_generator(*(function_definition(true)))
            .generate(sink, cls.functions, context)) return false;

         for(auto first = std::begin(cls.inherits)
               , last = std::end(cls.inherits); first != last; ++first)
           {
             attributes::klass_def klass(get_klass(*first));
             
             if(!as_generator(*(function_definition(true)))
                .generate(sink, klass.functions, context)) return false;
           }

         
         if(!as_generator("}\n").generate(sink, attributes::unused, context)) return false;
       }

     std::size_t function_count = cls.functions.size();
     for(auto first = std::begin(cls.inherits)
           , last = std::end(cls.inherits); first != last; ++first)
       {
         attributes::klass_def klass(get_klass(*first));
         function_count += klass.functions.size();
       }
     // function_count--;

     int function_registration_index = 0;
     auto index_generator = [&function_registration_index]
       {
         return function_registration_index++;
       };
     
     // Native Inherit class
     if(class_type == "class")
       {
         if(!as_generator
            (
             "public " << class_type << " " << string << "NativeInherit {\n"
             << scope_tab << "public static byte class_initializer(IntPtr klass)\n"
             << scope_tab << "{\n"
             << scope_tab << scope_tab << "Efl_Op_Description[] descs = new Efl_Op_Description[" << grammar::int_ << "];\n"
             << *(function_registration(index_generator, cls))
            )
            .generate(sink, std::make_tuple(cls.cxx_name, function_count, cls.functions), context))
           return false;
         for(auto first = std::begin(cls.inherits)
               , last = std::end(cls.inherits); first != last; ++first)
           {
             attributes::klass_def klass(get_klass(*first));
             if(!as_generator(*(function_registration(index_generator, cls)))
                .generate(sink, klass.functions, context)) return false;
           }

         if(!as_generator
            (   scope_tab << scope_tab << "IntPtr descs_ptr = Marshal.AllocHGlobal(Marshal.SizeOf(descs[0])*" << function_count << ");\n"
             << scope_tab << scope_tab << "IntPtr ptr = descs_ptr;\n"
             << scope_tab << scope_tab << "for(int i = 0; i != " << function_count << "; ++i)\n"
             << scope_tab << scope_tab << "{\n"
             << scope_tab << scope_tab << scope_tab << "Marshal.StructureToPtr(descs[i], ptr, false);\n"
             << scope_tab << scope_tab << scope_tab << "ptr = IntPtr.Add(ptr, Marshal.SizeOf(descs[0]));\n"
             << scope_tab << scope_tab << "}\n"
             << scope_tab << scope_tab << "Efl_Object_Ops ops;\n"
             << scope_tab << scope_tab << "ops.descs = descs_ptr;\n"
             << scope_tab << scope_tab << "ops.count = (UIntPtr)" << function_count << ";\n"
             << scope_tab << scope_tab << "IntPtr ops_ptr = Marshal.AllocHGlobal(Marshal.SizeOf(ops));\n"
             << scope_tab << scope_tab << "Marshal.StructureToPtr(ops, ops_ptr, false);\n"
             << scope_tab << scope_tab << "efl.eo.Globals.efl_class_functions_set(klass, ops_ptr, IntPtr.Zero);\n"
            ).generate(sink, attributes::unused, context)) return false;
         
         
         if(!as_generator(scope_tab << scope_tab << "return 1;\n"
                          << scope_tab << "}\n")
            .generate(sink, attributes::unused, context)) return false;
     
         if(!as_generator(*(native_function_definition(cls)))
            .generate(sink, cls.functions, context)) return false;

         for(auto first = std::begin(cls.inherits)
               , last = std::end(cls.inherits); first != last; ++first)
           {
             attributes::klass_def klass(get_klass(*first));
             
             if(!as_generator(*(native_function_definition(cls)))
                .generate(sink, klass.functions, context)) return false;
           }
         
         if(!as_generator("}\n").generate(sink, attributes::unused, context)) return false;
       }
     
     auto close_namespace = *(lit("} ")) << "\n";
     if(!as_generator(close_namespace).generate(sink, namespaces, context)) return false;
     
     return true;
   }

   template <typename OutputIterator, typename Context>
   bool generate_events_registration(OutputIterator sink, attributes::klass_def const& cls, Context const& context) const
   {
     // Event proxy registration
     if (!as_generator(
            scope_tab << "private void register_event_proxies()\n"
            << scope_tab << "{\n"
         )
         .generate(sink, NULL, context))
         return false;

     // Generate event registrations here

     // Assigning the delegates
     for (auto&& e : cls.events)
       {
           if (!as_generator(scope_tab << scope_tab << "evt_" << grammar::string_replace(',', '_') << "_delegate = "
                       << "new efl.Event_Cb(on" << grammar::string_replace(',', '_') << "Callback);\n")
                   .generate(sink, std::make_tuple(e.name, e.name), context))
                return false;
       }
     

     if (!as_generator(
            scope_tab << "}\n"
                 ).generate(sink, NULL, context))
         return false;

     return true;
   }

   // Helper method to avoid multipler as_generator calls when mixing case strings
   static std::string to_uppercase(std::string s)
   {
       std::transform(s.begin(), s.end(), s.begin(), ::toupper);
       return s;
   }

   template <typename OutputIterator, typename Context>
   bool generate_events(OutputIterator sink, attributes::klass_def const& cls, Context const& context) const
   {
     // Callback registration functions
     if (!as_generator(
            scope_tab << "private bool add_cpp_event_handler(string key, efl.Event_Cb evt_delegate) {\n"
            << scope_tab << scope_tab << "int event_count = 0;\n"
            << scope_tab << scope_tab << "if (!event_cb_count.TryGetValue(key, out event_count))\n"
            << scope_tab << scope_tab << scope_tab << "event_cb_count[key] = event_count;\n"
            << scope_tab << scope_tab << "if (event_count == 0) {\n"
            << scope_tab << scope_tab << scope_tab << "efl.kw_event.Description desc = new efl.kw_event.Description(key);\n"
            << scope_tab << scope_tab << scope_tab << "bool result = efl.eo.Globals.efl_event_callback_priority_add(handle, desc, 0, evt_delegate, System.IntPtr.Zero);\n"
            << scope_tab << scope_tab << scope_tab << "if (!result) {\n"
            << scope_tab << scope_tab << scope_tab << scope_tab << "Console.WriteLine(\"Failed to add event proxy for event ${key}\");\n"
            << scope_tab << scope_tab << scope_tab << scope_tab << "return false;\n"
            << scope_tab << scope_tab << scope_tab << "}\n"
            << scope_tab << scope_tab << "} \n"
            << scope_tab << scope_tab << "event_cb_count[key]++;\n"
            << scope_tab << scope_tab << "return true;\n"
            << scope_tab << "}\n"
            << scope_tab << "private bool remove_cpp_event_handler(string key, efl.Event_Cb evt_delegate) {\n"
            << scope_tab << scope_tab << "int event_count = 0;\n"
            << scope_tab << scope_tab << "if (!event_cb_count.TryGetValue(key, out event_count))\n"
            << scope_tab << scope_tab << scope_tab << "event_cb_count[key] = event_count;\n"
            << scope_tab << scope_tab << "if (event_count == 1) {\n"
            << scope_tab << scope_tab << scope_tab << "efl.kw_event.Description desc = new efl.kw_event.Description(key);\n"
            << scope_tab << scope_tab << scope_tab << "bool result = efl.eo.Globals.efl_event_callback_del(handle, desc, evt_delegate, System.IntPtr.Zero);\n"
            << scope_tab << scope_tab << scope_tab << "if (!result) {\n"
            << scope_tab << scope_tab << scope_tab << scope_tab << "Console.WriteLine(\"Failed to remove event proxy for event ${key}\");\n"
            << scope_tab << scope_tab << scope_tab << scope_tab << "return false;\n"
            << scope_tab << scope_tab << scope_tab << "}\n"
            << scope_tab << scope_tab << "} else if (event_count == 0) {\n"
            << scope_tab << scope_tab << scope_tab << "Console.WriteLine(\"Trying to remove proxy for event ${key} when there is nothing registered.\");\n"
            << scope_tab << scope_tab << scope_tab << "return false;\n"
            << scope_tab << scope_tab << "} \n"
            << scope_tab << scope_tab << "event_cb_count[key]--;\n"
            << scope_tab << scope_tab << "return true;\n"
            << scope_tab << "}\n"
            )
             .generate(sink, NULL, context))
         return false;

     // Self events
     for (auto&& e : cls.events)
       {
           std::string upper_name = to_uppercase(e.name);
           std::string name = e.name;
           /* name[0] = toupper(name[0]); */

           // Wrapper event declaration
          if(!as_generator(
                scope_tab << "protected event EventHandler " << grammar::string_replace(',', '_') << ";\n"
                << scope_tab << "protected void On" << grammar::string_replace(',', '_') << "(EventArgs e)\n"
                << scope_tab << "{\n"
                << scope_tab << scope_tab << "EventHandler evt = " << grammar::string_replace(',', '_') << ";\n"
                << scope_tab << scope_tab << "if (evt != null) { evt(this, e); }\n"
                << scope_tab << "}\n")
                  .generate(sink, std::make_tuple(upper_name, name, upper_name), context))
              return false;

          // Eo Event callback
          if(!as_generator(
                scope_tab << "public void on" << grammar::string_replace(',', '_') << "Callback(System.IntPtr data, System.IntPtr evt)\n"
                << scope_tab << "{\n"
                << scope_tab << scope_tab << "On" << grammar::string_replace(',', '_') << "(EventArgs.Empty);\n"
                << scope_tab << "}\n")
                  .generate(sink, std::make_tuple(name, name, name), context))
              return false;


          // FIXME Maybe allow contexts mixing cases instead of these successive as_generator?
          if(!as_generator(scope_tab
                << "efl.Event_Cb evt_" << grammar::string_replace(',', '_') << "_delegate;\n").generate(sink, e.name, context))
              return false;
          if(!as_generator(scope_tab
                << "event EventHandler " << string << ".")
                  .generate(sink, cls.cxx_name, context))
              return false;

          // Open add block body
          // FIXME Add locking
          if (!as_generator(grammar::string_replace(',', '_') << " {\n"
                << scope_tab << scope_tab << "add {\n")
                  .generate(sink, e.name, add_upper_case_context(context)))
              return false;

          // Add block body
          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "string key = \"_" << string << "\";\n")
                  .generate(sink, e.c_name, add_upper_case_context(context)))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "if (add_cpp_event_handler(key, this.evt_"
                      << grammar::string_replace(',', '_') << "_delegate))\n")
                  .generate(sink, e.name, context))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab << scope_tab
                      << grammar::string_replace(',', '_') << " += value;\n")
                  .generate(sink, e.name, add_upper_case_context(context)))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "else\n"
                      << scope_tab << scope_tab << scope_tab << scope_tab
                      << "Console.WriteLine(\"Error adding proxy for event ${key}\");\n")
                  .generate(sink, NULL, context))
              return false;

          // Close add block and open remove block
          if (!as_generator(scope_tab << scope_tab << "}\n"
                << scope_tab << scope_tab << "remove {\n")
                  .generate(sink, NULL, context))
              return false;

          // Remove block body
          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "string key = \"_" << string << "\";\n")
                  .generate(sink, e.c_name, add_upper_case_context(context)))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "if (remove_cpp_event_handler(key, this.evt_"
                      << grammar::string_replace(',', '_') << "_delegate))\n")
                  .generate(sink, e.name, context))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab << scope_tab
                      << grammar::string_replace(',', '_') << " -= value;\n")
                  .generate(sink, e.name, add_upper_case_context(context)))
              return false;

          if (!as_generator(
                      scope_tab << scope_tab << scope_tab
                      << "else\n"
                      << scope_tab << scope_tab << scope_tab << scope_tab
                      << "Console.WriteLine(\"Error removing proxy for event ${key}\");\n")
                  .generate(sink, NULL, context))
              return false;

          // Close the remove block
          if (!as_generator(scope_tab << scope_tab << "}\n"
                << scope_tab << "}\n")
                .generate(sink, NULL, context))
              return false;
       }

     // Inherited events
     for (auto&& c : cls.inherits)
       {
          attributes::klass_def klass(get_klass(c));

          // FIXME Enable inherited events registration. Beware of conflicting events
          for (auto&& e : klass.events)
            {
               if(!as_generator(scope_tab
                     << "event EventHandler " << *(lower_case[string] << ".")
                     << string << ".")
                       .generate(sink, std::make_tuple(escape_namespace(klass.namespaces), klass.cxx_name), context))
                   return false;
               if (!as_generator(grammar::string_replace(',', '_') << " {\n"
                     << scope_tab << scope_tab << "add {} \n"
                     << scope_tab << scope_tab << "remove {}\n"
                     << scope_tab << "}\n")
                 .generate(sink, e.name, add_upper_case_context(context)))
                   return false;
            }
       }
     return true;
   }
};

struct klass const klass = {};

}

namespace efl { namespace eolian { namespace grammar {
  
template <>
struct is_eager_generator<struct ::eolian_mono::klass> : std::true_type {};
template <>
struct is_generator<struct ::eolian_mono::klass> : std::true_type {};

namespace type_traits {
template <>
struct attributes_needed<struct ::eolian_mono::klass> : std::integral_constant<int, 1> {};
}
      
} } }

#endif
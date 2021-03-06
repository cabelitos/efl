#ifndef EOLIAN_CXX_CASE_HH
#define EOLIAN_CXX_CASE_HH

#include "grammar/context.hpp"
#include "grammar/generator.hpp"

namespace efl { namespace eolian { namespace grammar {

struct upper_case_tag {};
struct lower_case_tag {};
      
template <typename Context>
context_cons<upper_case_tag, Context>
add_upper_case_context(Context const& context)
{
  return context_add_tag(upper_case_tag{}, context);
}

template <typename Context>
context_cons<lower_case_tag, Context>
add_lower_case_context(Context const& context)
{
  return context_add_tag(lower_case_tag{}, context);
}

template <typename G>
struct lower_case_generator
{
  lower_case_generator(G g) : g(g) {}
  
  template <typename OutputIterator, typename Attribute, typename Context>
  bool generate(OutputIterator sink, Attribute const& attribute, Context const& context) const
  {
    return as_generator(g).generate(sink, attribute, add_lower_case_context(context));
  }

  G g;
};

template <typename G>
struct upper_case_generator
{
  upper_case_generator(G g) : g(g) {}
  
  template <typename OutputIterator, typename Attribute, typename Context>
  bool generate(OutputIterator sink, Attribute const& attribute, Context const& context) const
  {
    return as_generator(g).generate(sink, attribute, add_upper_case_context(context));
  }

  G g;
};

template <typename G>
struct is_eager_generator<lower_case_generator<G>> : std::true_type {};
template <typename G>
struct is_eager_generator<upper_case_generator<G>> : std::true_type {};

namespace type_traits {
template <typename G>
struct attributes_needed<lower_case_generator<G>> : attributes_needed<G> {};
template <typename G>
struct attributes_needed<upper_case_generator<G>> : attributes_needed<G> {};
}
      
struct lower_case_directive
{
  template <typename G>
  lower_case_generator<G> operator[](G&& g) const
  {
    return lower_case_generator<G>{g};
  }
} const lower_case = {};

struct upper_case_directive
{
  template <typename G>
  upper_case_generator<G> operator[](G&& g) const
  {
    return upper_case_generator<G>{g};
  }
} const upper_case = {};
      
} } }

#endif

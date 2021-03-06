#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eina.h>
#include "eolian_database.h"

void
database_implement_del(Eolian_Implement *impl)
{
   if (!impl) return;
   if (impl->base.file) eina_stringshare_del(impl->base.file);
   if (impl->full_name) eina_stringshare_del(impl->full_name);
   database_doc_del(impl->common_doc);
   database_doc_del(impl->get_doc);
   database_doc_del(impl->set_doc);
   free(impl);
}

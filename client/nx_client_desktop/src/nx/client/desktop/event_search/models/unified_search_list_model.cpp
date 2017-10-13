#include "unified_search_list_model.h"
#include "private/unified_search_list_model_p.h"

namespace nx {
namespace client {
namespace desktop {

UnifiedSearchListModel::UnifiedSearchListModel(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

UnifiedSearchListModel::~UnifiedSearchListModel()
{
}

} // namespace
} // namespace client
} // namespace nx

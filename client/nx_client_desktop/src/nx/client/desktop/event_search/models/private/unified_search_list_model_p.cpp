#include "unified_search_list_model_p.h"

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

UnifiedSearchListModel::Private::Private(UnifiedSearchListModel* q):
    base_type(),
    QnWorkbenchContextAware(q),
    q(q),
    m_helper(new vms::event::StringsHelper(commonModule()))
{
}

UnifiedSearchListModel::Private::~Private()
{
}

} // namespace
} // namespace client
} // namespace nx

#include "director.h"

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vmx::client::desktop {

using namespace vms::client::desktop::ui;

QScopedPointer<Director> Director::m_director;

Director* Director::instance()
{
    NX_ASSERT(m_director);
    return m_director.data();
}

void Director::quit()
{
    m_context->action(action::ExitAction)->trigger();
}

void Director::createInstance(QnWorkbenchContext* context)
{
    NX_ASSERT(!m_director, "Attempted to create duplicate Director");
    m_director.reset(new Director(context));
}

Director::Director(QnWorkbenchContext* context):
    m_context(context)
{
}

} // namespace nx::vmx::client::desktop
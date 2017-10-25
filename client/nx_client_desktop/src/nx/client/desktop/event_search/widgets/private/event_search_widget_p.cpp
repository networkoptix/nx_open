#include "event_search_widget_p.h"

#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>

#include <ui/style/helper.h>
#include <ui/widgets/common/search_line_edit.h>
#include <nx/client/desktop/event_search/models/unified_search_list_model.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx {
namespace client {
namespace desktop {

EventSearchWidget::Private::Private(EventSearchWidget* q):
    QObject(),
    QnWorkbenchContextAware(q),
    q(q),
    m_model(new UnifiedSearchListModel(this)),
    m_headerWidget(new QWidget(q)),
    m_eventRibbon(new EventRibbon(q))
{
    auto line = new QFrame(q);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    auto layout = new QVBoxLayout(q);
    layout->addWidget(m_headerWidget);
    layout->addWidget(line);
    layout->addWidget(m_eventRibbon);
    layout->setSpacing(0);

    m_eventRibbon->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    auto headerLayout = new QVBoxLayout(m_headerWidget);
    auto searchLineEdit = new QnSearchLineEdit(m_headerWidget);
    headerLayout->addWidget(searchLineEdit);
    headerLayout->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);

    m_eventRibbon->setModel(m_model); // TODO: #vkutin Use filter proxy model.
}

EventSearchWidget::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventSearchWidget::Private::camera() const
{
    return m_camera;
}

void EventSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;
}

} // namespace
} // namespace client
} // namespace nx

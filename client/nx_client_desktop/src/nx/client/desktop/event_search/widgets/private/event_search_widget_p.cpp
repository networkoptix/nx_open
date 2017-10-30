#include "event_search_widget_p.h"

#include <chrono>

#include <QtWidgets/QFrame>
#include <QtWidgets/QVBoxLayout>

#include <ui/models/sort_filter_list_model.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/search_line_edit.h>

#include <nx/client/desktop/event_search/models/unified_search_list_model.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static const auto kFilterDelay = std::chrono::milliseconds(250);

class EventSortFilterModel: public QnSortFilterListModel
{
public:
    using QnSortFilterListModel::QnSortFilterListModel;

    bool lessThan(const QModelIndex& sourceLeft, const QModelIndex& sourceRight) const
    {
        return sourceLeft.data(Qn::TimestampRole).value<qint64>()
            > sourceRight.data(Qn::TimestampRole).value<qint64>();
    }
};

} // namespace

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
    searchLineEdit->setTextChangedSignalFilterMs(std::chrono::milliseconds(kFilterDelay).count());

    headerLayout->addWidget(searchLineEdit);
    headerLayout->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);

    auto sortModel = new EventSortFilterModel(this);
    sortModel->setSourceModel(m_model);
    sortModel->setFilterRole(Qn::ResourceSearchStringRole);

    connect(searchLineEdit, &QnSearchLineEdit::textChanged,
        sortModel, &EventSortFilterModel::setFilterWildcard);

    m_eventRibbon->setModel(sortModel);
}

EventSearchWidget::Private::~Private()
{
}

QnVirtualCameraResourcePtr EventSearchWidget::Private::camera() const
{
    return m_model->camera();
}

void EventSearchWidget::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    m_model->setCamera(camera);
}

} // namespace
} // namespace client
} // namespace nx

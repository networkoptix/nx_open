#include "event_search_widget_p.h"

#include <QtWidgets/QLayout>

#include <ui/models/sort_filter_list_model.h>
#include <ui/widgets/common/search_line_edit.h>

#include <nx/client/desktop/event_search/models/unified_search_list_model.h>

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
    base_type(q),
    q(q),
    m_model(new UnifiedSearchListModel(this))
{
    auto searchLineEdit = new QnSearchLineEdit(m_headerWidget);
    searchLineEdit->setTextChangedSignalFilterMs(std::chrono::milliseconds(kFilterDelay).count());

    m_headerWidget->layout()->addWidget(searchLineEdit);

    auto sortModel = new EventSortFilterModel(this);
    sortModel->setSourceModel(m_model);
    sortModel->setFilterRole(Qn::ResourceSearchStringRole);
    sortModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(searchLineEdit, &QnSearchLineEdit::textChanged,
        sortModel, &EventSortFilterModel::setFilterWildcard);

    connectEventRibbonToModel(m_ribbon, m_model, sortModel);
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

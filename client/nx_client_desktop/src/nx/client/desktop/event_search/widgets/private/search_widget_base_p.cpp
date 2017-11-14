#include "search_widget_base_p.h"

#include <QtCore/QAbstractProxyModel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include <ui/style/helper.h>
#include <utils/common/event_processors.h>

#include <nx/client/desktop/common/models/subset_list_model.h>
#include <nx/client/desktop/event_search/models/event_list_model.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx {
namespace client {
namespace desktop {
namespace detail {

EventSearchWidgetPrivateBase::EventSearchWidgetPrivateBase(QWidget* q):
    QObject(),
    QnWorkbenchContextAware(q),
    m_headerWidget(new QWidget(q)),
    m_ribbon(new EventRibbon(q))
{
    auto line = new QFrame(q);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);

    auto layout = new QVBoxLayout(q);
    layout->addWidget(m_headerWidget);
    layout->addWidget(line);
    layout->addWidget(m_ribbon);
    layout->setSpacing(0);

    m_ribbon->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

    auto headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding,
        style::Metrics::kStandardPadding);
}

EventSearchWidgetPrivateBase::~EventSearchWidgetPrivateBase()
{
}

void EventSearchWidgetPrivateBase::connectEventRibbonToModel(
    EventRibbon* ribbon,
    EventListModel* model,
    QAbstractProxyModel* proxyModel)
{
    if (proxyModel)
        ribbon->setModel(new SubsetListModel(proxyModel, 0, QModelIndex(), this));
    else
        ribbon->setModel(model);

    const auto fetchMoreIfNeeded =
        [ribbon, model]()
        {
            if (!ribbon->isVisible())
                return;

            const auto scrollBar = ribbon->scrollBar();
            if (scrollBar->isVisible() && scrollBar->value() < scrollBar->maximum())
                return;

            if (!model->canFetchMore())
                return;

            model->fetchMore();
        };

    if (proxyModel)
    {
        connect(proxyModel, &QAbstractItemModel::rowsRemoved,
            this, fetchMoreIfNeeded, Qt::QueuedConnection);
    }

    connect(ribbon->scrollBar(), &QScrollBar::valueChanged, this, fetchMoreIfNeeded);
    connect(model, &EventListModel::fetchFinished, this, fetchMoreIfNeeded);
    installEventHandler(ribbon, QEvent::Show, this, fetchMoreIfNeeded);
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

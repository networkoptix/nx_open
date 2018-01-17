#include "search_widget_base_p.h"

#include <QtCore/QAbstractListModel>
#include <QtWidgets/QFrame>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include <ui/style/helper.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

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

void EventSearchWidgetPrivateBase::setModel(QAbstractListModel* model)
{
    m_ribbon->setModel(model);

    connect(model, &QAbstractItemModel::rowsRemoved,
        this, &EventSearchWidgetPrivateBase::fetchMoreIfNeeded, Qt::QueuedConnection);

    connect(m_ribbon->scrollBar(), &QScrollBar::valueChanged,
        this, &EventSearchWidgetPrivateBase::fetchMoreIfNeeded, Qt::QueuedConnection);

    installEventHandler(m_ribbon, QEvent::Show,
        this, &EventSearchWidgetPrivateBase::fetchMoreIfNeeded);
}

void EventSearchWidgetPrivateBase::fetchMoreIfNeeded()
{
    if (!m_ribbon->isVisible())
        return;

    const auto scrollBar = m_ribbon->scrollBar();
    if (scrollBar->isVisible() && scrollBar->value() < scrollBar->maximum())
        return;

    if (!m_ribbon->model()->canFetchMore(QModelIndex()))
        return;

    m_ribbon->model()->fetchMore(QModelIndex());
}

void EventSearchWidgetPrivateBase::queueFetchMoreIfNeeded(int delayMs)
{
    executeDelayedParented([this]() { fetchMoreIfNeeded(); }, delayMs, this);
}

} // namespace detail
} // namespace desktop
} // namespace client
} // namespace nx

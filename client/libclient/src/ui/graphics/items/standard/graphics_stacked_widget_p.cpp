#include "graphics_stacked_widget.h"
#include "graphics_stacked_widget_p.h"

#include <limits>

#include <QtCore/private/qobject_p.h>

#include <ui/common/geometry.h>

#include <utils/common/event_processors.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

namespace {

static constexpr int kNoIndex = -1;

} // namespace

QnGraphicsStackedWidgetPrivate::QnGraphicsStackedWidgetPrivate(QnGraphicsStackedWidget* main):
    base_type(),
    q_ptr(main)
{
    installEventHandler(main, QEvent::ContentsRectChange,
        this, &QnGraphicsStackedWidgetPrivate::updateGeometries);

    connect(main, &QnGraphicsStackedWidget::geometryChanged,
        this, &QnGraphicsStackedWidgetPrivate::updateGeometries);

    //TODO: #vkutin Think how to handle child widgets size hint change
}

QnGraphicsStackedWidgetPrivate::~QnGraphicsStackedWidgetPrivate()
{
}

int QnGraphicsStackedWidgetPrivate::insertWidget(int index, QGraphicsWidget* widget)
{
    if (!widget) //< don't insert null widgets
        return kNoIndex;

    if (indexOf(widget) != kNoIndex) //< don't insert the same widget multiple times
        return kNoIndex;

    index = qBound(0, index, m_widgets.count());
    m_widgets.insert(index, widget);

    Q_Q(QnGraphicsStackedWidget);
    widget->setParent(q);
    widget->setParentItem(q);

    setWidgetGeometry(widget, q->contentsRect());

    connect(widget, &QGraphicsWidget::destroyed, this,
        [this, widget]() { removeWidget(widget); });

    if (m_currentIndex == kNoIndex)
    {
        NX_EXPECT(index == 0);
        setCurrentIndex(index);
    }
    else
    {
        if (m_currentIndex >= index)
            ++m_currentIndex;

        updateVisibility();
    }

    q->updateGeometry();
    return index;
}

QGraphicsWidget* QnGraphicsStackedWidgetPrivate::removeWidget(int index)
{
    if (index < 0 || index >= m_widgets.size())
        return nullptr;

    auto widget = m_widgets.takeAt(index);
    NX_EXPECT(widget);

    if (!QObjectPrivate::get(widget)->wasDeleted)
    {
        widget->setParentItem(nullptr);
        widget->setParent(nullptr);
        widget->disconnect(this);
    }

    if (m_currentIndex > index)
        --m_currentIndex;
    else if (m_currentIndex == index)
        m_currentIndex = qMin(m_widgets.size() - 1, qMax(0, m_currentIndex));

    Q_Q(QnGraphicsStackedWidget);

    updateVisibility();
    q->updateGeometry();

    emit q->currentChanged(widgetAt(m_currentIndex));

    return widget;
}

int QnGraphicsStackedWidgetPrivate::removeWidget(QGraphicsWidget* widget)
{
    const int index = indexOf(widget);
    if (index == kNoIndex)
        return kNoIndex;

    removeWidget(index);
    return index;
}

int QnGraphicsStackedWidgetPrivate::count() const
{
    return m_widgets.size();
}

int QnGraphicsStackedWidgetPrivate::indexOf(QGraphicsWidget* widget) const
{
    return m_widgets.indexOf(widget);
}

QGraphicsWidget* QnGraphicsStackedWidgetPrivate::widgetAt(int index) const
{
    return index >= 0 && index <= m_widgets.size()
        ? m_widgets[index]
        : nullptr;
}

Qt::Alignment QnGraphicsStackedWidgetPrivate::alignment(QGraphicsWidget* widget) const
{
    return m_alignments[widget];
}

void QnGraphicsStackedWidgetPrivate::setAlignment(QGraphicsWidget* widget, Qt::Alignment alignment)
{
    if (!widget)
        return;

    if (this->alignment(widget) == alignment)
        return;

    m_alignments[widget] = alignment;

    Q_Q(QnGraphicsStackedWidget);
    setWidgetGeometry(widget, q->contentsRect());
}

int QnGraphicsStackedWidgetPrivate::currentIndex() const
{
    return m_currentIndex;
}

QGraphicsWidget* QnGraphicsStackedWidgetPrivate::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_widgets.size())
        return nullptr;

    const auto currentWidget = m_widgets[index];

    if (m_currentIndex == index)
        return currentWidget;

    m_currentIndex = index;
    updateVisibility();

    Q_Q(QnGraphicsStackedWidget);
    emit q->currentChanged(currentWidget);

    return currentWidget;
}

QGraphicsWidget* QnGraphicsStackedWidgetPrivate::currentWidget() const
{
    NX_EXPECT(m_currentIndex >= 0 && m_currentIndex < m_widgets.size()
        || m_currentIndex == kNoIndex);

    return m_currentIndex >= 0 && m_currentIndex < m_widgets.size()
        ? m_widgets[m_currentIndex]
        : nullptr;
}

int QnGraphicsStackedWidgetPrivate::setCurrentWidget(QGraphicsWidget* widget)
{
    const int index = indexOf(widget);
    if (index == kNoIndex)
        return kNoIndex;

    setCurrentIndex(index);
    return index;
}

QStackedLayout::StackingMode QnGraphicsStackedWidgetPrivate::stackingMode() const
{
    return m_stackingMode;
}

void QnGraphicsStackedWidgetPrivate::setStackingMode(QStackedLayout::StackingMode mode)
{
    if (m_stackingMode == mode)
        return;

    m_stackingMode = mode;
    updateVisibility();
}

void QnGraphicsStackedWidgetPrivate::setWidgetGeometry(QGraphicsWidget* widget, const QRectF& geometry)
{
    const auto size = geometry.size()
        .expandedTo(widget->minimumSize())
        .boundedTo(widget->maximumSize());

    if (qFuzzyEquals(size, geometry.size()))
        widget->setGeometry(geometry);
    else
        widget->setGeometry(QnGeometry::aligned(size, geometry, alignment(widget)));
}

void QnGraphicsStackedWidgetPrivate::updateGeometries()
{
    Q_Q(QnGraphicsStackedWidget);
    const auto rect = q->contentsRect();

    for (auto widget: m_widgets)
        setWidgetGeometry(widget, rect);
}

QSizeF QnGraphicsStackedWidgetPrivate::sizeHint(Qt::SizeHint which, const QSizeF& constraint) const
{
    switch (which)
    {
        case Qt::MinimumSize:
        case Qt::PreferredSize:
        {
            QSizeF size;
            for (const auto widget: m_widgets)
                size = size.expandedTo(widget->effectiveSizeHint(which, constraint));

            return size;
        }

        case Qt::MaximumSize:
        {
            static constexpr qreal kHuge = std::numeric_limits<qreal>::max();
            QSizeF size(kHuge, kHuge);

            for (const auto widget: m_widgets)
                size = size.boundedTo(widget->effectiveSizeHint(which, constraint));

            return size;
        }

        default:
            return QSizeF();
    }
}

void QnGraphicsStackedWidgetPrivate::updateVisibility()
{
    if (m_stackingMode == QStackedLayout::StackOne)
    {
        for (int i = 0; i < m_widgets.count(); ++i)
            m_widgets[i]->setVisible(i == m_currentIndex);
    }
    else // m_stackingMode == QStackedLayout::StackAll
    {
        for (int i = 0; i < m_widgets.count(); ++i)
        {
            m_widgets[i]->setVisible(true);
            m_widgets[i]->setZValue(i == m_currentIndex ? m_widgets.count() : i);
        }
    }
}

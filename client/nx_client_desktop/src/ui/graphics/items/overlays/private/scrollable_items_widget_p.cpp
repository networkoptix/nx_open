#include "scrollable_items_widget_p.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsWidget>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>

#include <ui/graphics/items/generic/graphics_scroll_area.h>
#include <ui/graphics/items/overlays/scrollable_items_widget.h>

QnScrollableItemsWidgetPrivate::QnScrollableItemsWidgetPrivate(
    QnScrollableItemsWidget* parent)
    :
    base_type(parent),
    q_ptr(parent),
    m_scrollArea(new QnGraphicsScrollArea(parent)),
    m_contentLayout(new QGraphicsLinearLayout(Qt::Vertical))
{
    auto contentWidget = new QGraphicsWidget(parent);
    contentWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    contentWidget->setLayout(m_contentLayout);
    contentWidget->setAcceptedMouseButtons(Qt::NoButton);
    m_scrollArea->setContentWidget(contentWidget);

    auto mainLayout = new QGraphicsLinearLayout(parent);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addItem(m_scrollArea);

    connect(contentWidget, &QGraphicsWidget::widthChanged,
        this, &QnScrollableItemsWidgetPrivate::updateContentPosition);
    connect(contentWidget, &QGraphicsWidget::geometryChanged,
        parent, &QnScrollableItemsWidget::updateGeometry);
    connect(parent, &QGraphicsWidget::geometryChanged,
        this, &QnScrollableItemsWidgetPrivate::updateContentPosition);

    connect(contentWidget, &QGraphicsWidget::widthChanged,
        parent, &QnScrollableItemsWidget::contentWidthChanged);
    connect(contentWidget, &QGraphicsWidget::heightChanged,
        parent, &QnScrollableItemsWidget::contentHeightChanged);
}

Qt::Alignment QnScrollableItemsWidgetPrivate::alignment() const
{
    return m_alignment;
}

void QnScrollableItemsWidgetPrivate::setAlignment(Qt::Alignment alignment)
{
    if (m_alignment == alignment)
        return;

    const auto oldAlignment = m_alignment;
    m_alignment = alignment;

    m_scrollArea->setAlignment(alignment);

    if ((oldAlignment & Qt::AlignHorizontal_Mask) == (alignment & Qt::AlignHorizontal_Mask))
        return;

    for (int i = 0; i < m_contentLayout->count(); ++i)
        m_contentLayout->setAlignment(m_contentLayout->itemAt(i), alignment);

    updateContentPosition();
}

void QnScrollableItemsWidgetPrivate::updateContentPosition()
{
    const auto contentGeometry = m_scrollArea->contentWidget()->geometry();

    const qreal left =
        [=]() -> qreal
        {
            switch (m_alignment & Qt::AlignHorizontal_Mask)
            {
                case Qt::AlignLeft:
                    return 0.0;

                case Qt::AlignRight:
                    return m_scrollArea->size().width() - contentGeometry.width();

                default: // everything else is handled as Qt::AlignHCenter
                    return (m_scrollArea->size().width() - contentGeometry.width()) / 2.0;
            }
        }();

    if (qFuzzyEquals(contentGeometry.left(), left))
        return;

    m_scrollArea->contentWidget()->setPos(left, contentGeometry.top());
}

QSizeF QnScrollableItemsWidgetPrivate::contentSizeHint(Qt::SizeHint which,
    const QSizeF& constraint) const
{
    return m_scrollArea->contentWidget()->effectiveSizeHint(which, constraint);
}

QnUuid QnScrollableItemsWidgetPrivate::insertItem(int index, QGraphicsWidget* item, const QnUuid& externalId)
{
    NX_EXPECT(item);
    if (!item)
        return QnUuid();

    const QnUuid id = externalId.isNull()
        ? QnUuid::createUuid()
        : externalId;

    if (m_items.contains(id))
        return QnUuid();

    item->setParent(m_scrollArea->contentWidget());
    item->setParentItem(m_scrollArea->contentWidget());

    connect(item, &QObject::destroyed, this,
        [this, id]()
        {
            /* An item should be connected to this lambda only
               if it's in the hash with exactly this id. */
            auto iter = m_items.find(id);
            const bool thisItem = iter != m_items.end() && iter.value() == sender();
            NX_EXPECT(thisItem);
            if (thisItem)
                m_items.erase(iter);
        });

    m_items[id] = item;
    m_contentLayout->insertItem(index, item);
    m_contentLayout->setAlignment(item, m_alignment);

    return id;
}

QGraphicsWidget* QnScrollableItemsWidgetPrivate::takeItem(const QnUuid& id)
{
    auto iter = m_items.find(id);
    if (iter == m_items.end())
        return nullptr;

    auto result = iter.value();
    m_items.erase(iter);

    NX_EXPECT(result);
    if (!result)
        return nullptr;

    m_contentLayout->removeItem(result);

    result->disconnect(this);
    result->setParent(nullptr);
    result->setParentItem(nullptr);
    return result;
}

void QnScrollableItemsWidgetPrivate::clear()
{
    /* Do not iterate through m_items because each item
       being deleted will remove itself from m_items. */
    for (auto item: m_items.values())
        delete item;

    NX_EXPECT(m_items.empty());
    m_items.clear();
}

int QnScrollableItemsWidgetPrivate::count() const
{
    NX_EXPECT(m_items.size() == m_contentLayout->count());
    return m_contentLayout->count();
}

QGraphicsWidget* QnScrollableItemsWidgetPrivate::item(int index) const
{
    if (index < 0 || index >= count())
        return nullptr;

    return qgraphicsitem_cast<QGraphicsWidget*>(m_contentLayout->itemAt(index)->graphicsItem());
}

QGraphicsWidget* QnScrollableItemsWidgetPrivate::item(const QnUuid& id) const
{
    const auto iter = m_items.find(id);
    return iter != m_items.end() ? iter.value() : nullptr;
}

qreal QnScrollableItemsWidgetPrivate::spacing() const
{
    return m_contentLayout->spacing();
}

void QnScrollableItemsWidgetPrivate::setSpacing(qreal value)
{
    m_contentLayout->setSpacing(value);
}

int QnScrollableItemsWidgetPrivate::lineHeight() const
{
    return m_scrollArea->lineHeight();
}

void QnScrollableItemsWidgetPrivate::setLineHeight(int value)
{
    m_scrollArea->setLineHeight(value);
}

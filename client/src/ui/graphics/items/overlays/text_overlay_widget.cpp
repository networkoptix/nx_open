#include "text_overlay_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/camera_bookmark.h>

#include <utils/common/delayed.h>
#include <ui/graphics/items/generic/graphics_scroll_area.h>

namespace 
{
    const int maximumBookmarkWidth = 250;
    const int layoutSpacing = 1;
}

///

QnOverlayTextItemData::QnOverlayTextItemData(const QnUuid &initId
    , const QString &initText
    , QnHtmlTextItemOptions initItemOptions
    , int initTimeout)
    : id(initId)
    , text(initText)
    , itemOptions(initItemOptions)
    , timeout(initTimeout)
{
}

bool QnOverlayTextItemData::operator == (const QnOverlayTextItemData &rhs) const
{
    return ((id == rhs.id)
        && (text == rhs.text)
        && (timeout == rhs.timeout)
        && (itemOptions.autosize == rhs.itemOptions.autosize)
        && (itemOptions.backgroundColor == rhs.itemOptions.backgroundColor)
        && (itemOptions.borderRadius == rhs.itemOptions.borderRadius)
        && (itemOptions.maxWidth == rhs.itemOptions.maxWidth)
        && (itemOptions.padding == rhs.itemOptions.padding));
}

bool QnOverlayTextItemData::operator != (const QnOverlayTextItemData &rhs) const
{
    return !(*this == rhs);
}

///

class QnTextOverlayWidgetPrivate 
{
    Q_DECLARE_PUBLIC(QnTextOverlayWidget)
    QnTextOverlayWidget *q_ptr;

public:
    QnTextOverlayWidgetPrivate(QnTextOverlayWidget *parent);
    
    ~QnTextOverlayWidgetPrivate();

    void resetItemsData();

    void addItemData(const QnOverlayTextItemData &data
        , bool updatePos);

    void removeItemData(const QnUuid &id);

    void setItemsData(const QnOverlayTextItemDataList &data);

private:
    void updatePositions();

private:
    QGraphicsWidget *m_contentWidget;
    QnGraphicsScrollArea *m_scrollArea;
    QGraphicsLinearLayout *m_mainLayout;

    typedef QSharedPointer<QnHtmlTextItem> QnHtmlTextItemPtr;
    typedef QPair<QnOverlayTextItemData, QnHtmlTextItemPtr> DataWidgetPair;
    QHash<QnUuid, DataWidgetPair> m_items;
};


QnTextOverlayWidgetPrivate::QnTextOverlayWidgetPrivate(QnTextOverlayWidget *parent)
    : q_ptr(parent)
    , m_contentWidget(nullptr)
    , m_scrollArea(nullptr)
    , m_mainLayout(nullptr)
{
}

QnTextOverlayWidgetPrivate::~QnTextOverlayWidgetPrivate()
{
}

void QnTextOverlayWidgetPrivate::resetItemsData()
{
    m_items.clear();

    updatePositions();
}

void QnTextOverlayWidgetPrivate::addItemData(const QnOverlayTextItemData &data
    , bool updatePos)
{
    const auto id = data.id;
    auto it = m_items.find(data.id);
    if (it == m_items.end())
    {
        const QnHtmlTextItemPtr textItem = QnHtmlTextItemPtr(new QnHtmlTextItem(data.text
            , data.itemOptions, m_contentWidget));

        it = m_items.insert(data.id
            , DataWidgetPair(data, textItem));
    }
    else
    {
        it->first = data;
        it->second->setHtml(data.text);
    }

    if (data.timeout > 0)
    {
        Q_Q(QnTextOverlayWidget);
        const auto removeItemFunc = [this, id]()
        {
            removeItemData(id);
        };

        executeDelayedParented(removeItemFunc, data.timeout, q);
    }
    if (updatePos)
        updatePositions();
}

void QnTextOverlayWidgetPrivate::removeItemData(const QnUuid &id)
{
    if (!m_items.remove(id))
        return;

    updatePositions();
}

void QnTextOverlayWidgetPrivate::setItemsData(const QnOverlayTextItemDataList &data)
{
    bool different = (m_items.size() !=  data.size());
    if (!different)
    {
        const auto it = std::find_if(data.begin(), data.end(), [this](const QnOverlayTextItemData &textItemData) -> bool
        {
            const auto it = m_items.find(textItemData.id);
            return ((it == m_items.end()) || (it->first != textItemData));
        });

        different = (it != data.end());
    }

    if (!different)
        return;

    resetItemsData();

    for (const auto textItemData: data)
        addItemData(textItemData, false);

    updatePositions();
}

void QnTextOverlayWidgetPrivate::updatePositions()
{
    int height = 0;
    for (const auto itemInnerData: m_items)
    {
        const auto textItemWidget = itemInnerData.second;
        textItemWidget->setPos(maximumBookmarkWidth - textItemWidget->size().width(), height);
        height += textItemWidget->size().height() + layoutSpacing;
    }

    m_contentWidget->resize(maximumBookmarkWidth, std::max(0, height - layoutSpacing));

    Q_Q(QnTextOverlayWidget);
    q->update();
}


///

QnTextOverlayWidget::QnTextOverlayWidget(QGraphicsWidget *parent)
    : GraphicsWidget(parent)
    , d_ptr(new QnTextOverlayWidgetPrivate(this))
{
    Q_D(QnTextOverlayWidget);

    setFlag(QGraphicsItem::ItemClipsChildrenToShape);
    setAcceptedMouseButtons(0);

    d->m_contentWidget = new QGraphicsWidget(this);

    d->m_scrollArea = new QnGraphicsScrollArea(this);
    d->m_scrollArea->setContentWidget(d->m_contentWidget);
    d->m_scrollArea->setMinimumWidth(maximumBookmarkWidth);
    d->m_scrollArea->setMaximumWidth(maximumBookmarkWidth);
    d->m_scrollArea->setAlignment(Qt::AlignBottom | Qt::AlignRight);

    d->m_mainLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    d->m_mainLayout->addStretch();
    d->m_mainLayout->addItem(d->m_scrollArea);
    setLayout(d->m_mainLayout);

    setContentsMargins(4, 20, 4, 20);
}

QnTextOverlayWidget::~QnTextOverlayWidget() 
{
}

void QnTextOverlayWidget::resetItemsData()
{
    Q_D(QnTextOverlayWidget);
    d->setItemsData(QnOverlayTextItemDataList());
}

void QnTextOverlayWidget::addItemData(const QnOverlayTextItemData &data)
{
    Q_D(QnTextOverlayWidget);
    d->addItemData(data, true);
}

void QnTextOverlayWidget::removeItemData(const QnUuid &id)
{
    Q_D(QnTextOverlayWidget);
    d->removeItemData(id);
}

void QnTextOverlayWidget::setItemsData(const QnOverlayTextItemDataList &data)
{
    Q_D(QnTextOverlayWidget);
    d->setItemsData(data);
}


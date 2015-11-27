#include "text_overlay_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/camera_bookmark.h>

#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnOverlayTextItemData, (eq), QnOverlayTextItemData_Fields);

///

class QnTextOverlayWidgetPrivate
{
    Q_DECLARE_PUBLIC(QnTextOverlayWidget)
    QnTextOverlayWidget *q_ptr;

public:
    QnTextOverlayWidgetPrivate(QnTextOverlayWidget *parent);

    ~QnTextOverlayWidgetPrivate();

    void clear();

    void addItem(const QnOverlayTextItemData &data
        , bool updatePos);

    void removeItem(const QnUuid &id);

    void setItems(const QnOverlayTextItemDataList &data);

private:
    void updatePositions();

private:
    QGraphicsWidget * const m_contentWidget;
    QnGraphicsScrollArea * const m_scrollArea;
    QGraphicsLinearLayout * const m_mainLayout;

    typedef QSharedPointer<QnHtmlTextItem> QnHtmlTextItemPtr;
    typedef QPair<QnOverlayTextItemData, QnHtmlTextItemPtr> DataWidgetPair;
    QHash<QnUuid, DataWidgetPair> m_items;
};


QnTextOverlayWidgetPrivate::QnTextOverlayWidgetPrivate(QnTextOverlayWidget *parent)
    : q_ptr(parent)
    , m_contentWidget(new QGraphicsWidget(parent))
    , m_scrollArea(new QnGraphicsScrollArea(parent))
    , m_mainLayout(new QGraphicsLinearLayout(Qt::Horizontal))
{
    m_scrollArea->setContentWidget(m_contentWidget);
    m_scrollArea->setMinimumWidth(maximumBookmarkWidth);
    m_scrollArea->setMaximumWidth(maximumBookmarkWidth);
    m_scrollArea->setAlignment(Qt::AlignBottom | Qt::AlignRight);

    m_mainLayout->addStretch();
    m_mainLayout->addItem(m_scrollArea);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
}

QnTextOverlayWidgetPrivate::~QnTextOverlayWidgetPrivate()
{
}

void QnTextOverlayWidgetPrivate::clear()
{
    m_items.clear();

    updatePositions();
}

void QnTextOverlayWidgetPrivate::addItem(const QnOverlayTextItemData &data
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
            removeItem(id);
        };

        executeDelayedParented(removeItemFunc, data.timeout, q);
    }
    if (updatePos)
        updatePositions();
}

void QnTextOverlayWidgetPrivate::removeItem(const QnUuid &id)
{
    if (!m_items.remove(id))
        return;

    updatePositions();
}

void QnTextOverlayWidgetPrivate::setItems(const QnOverlayTextItemDataList &data)
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

    clear();

    for (const auto textItemData: data)
        addItem(textItemData, false);


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
    setLayout(d->m_mainLayout);

    enum
    {
        kDefaultHorMargin = 0
        , kDefaultBottomMargin = 34
        , kDefaultTopMargin = 28
    };

    setContentsMargins(kDefaultHorMargin, kDefaultTopMargin
        , kDefaultHorMargin, kDefaultBottomMargin);
}

QnTextOverlayWidget::~QnTextOverlayWidget()
{
}

void QnTextOverlayWidget::clear()
{
    Q_D(QnTextOverlayWidget);
    d->setItems(QnOverlayTextItemDataList());
}

void QnTextOverlayWidget::addItem(const QnOverlayTextItemData &data)
{
    Q_D(QnTextOverlayWidget);
    d->addItem(data, true);
}

void QnTextOverlayWidget::removeItem(const QnUuid &id)
{
    Q_D(QnTextOverlayWidget);
    d->removeItem(id);
}

void QnTextOverlayWidget::setItems(const QnOverlayTextItemDataList &data)
{
    Q_D(QnTextOverlayWidget);
    d->setItems(data);
}


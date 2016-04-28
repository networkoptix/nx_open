#include "text_overlay_widget.h"

#include <QtCore/QTimer>
#include <QtWidgets/QGraphicsLinearLayout>

#include <core/resource/camera_bookmark.h>

#include <ui/graphics/items/generic/graphics_scroll_area.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>

#include <utils/common/delayed.h>
#include <utils/common/model_functions.h>

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

QnTextOverlayWidget::QnTextOverlayWidget(QGraphicsWidget *parent)
    : base_type(Qt::AlignRight, parent)
{
    enum
    {
        kDefaultHorMargin = 2
        , kDefaultBottomMargin = 2
        , kDefaultTopMargin = 28
    };

    setContentsMargins(kDefaultHorMargin, kDefaultTopMargin
        , kDefaultHorMargin, kDefaultBottomMargin);
}

QnTextOverlayWidget::~QnTextOverlayWidget()
{}

void QnTextOverlayWidget::addTextItem(const QnOverlayTextItemData &data)
{
    const auto id = data.id;

    removeTextItem(id);  // In case of update we should remove data (and cancel timer event, if any)
    QnHtmlTextItem* textItem = new QnHtmlTextItem(data.text, data.itemOptions);
    textItem->setProperty(Qn::NoBlockMotionSelection, true);

    m_textItems.insert(id, data);
    addItem(textItem, id);

    if (data.timeout > 0)
    {
        const auto removeItemFunc = [this, id]()
        {
            removeTextItem(id);
        };

        m_delayedRemoveTimers.insert(id, executeDelayedParented(removeItemFunc, data.timeout, this));
    }

}

void QnTextOverlayWidget::removeTextItem(const QnUuid &id)
{
    if (QPointer<QTimer> timer = m_delayedRemoveTimers.take(id)) {
        timer->stop();
        timer->deleteLater();
    }

    m_textItems.remove(id);
    removeItem(id);
}

void QnTextOverlayWidget::setTextItems(const QnOverlayTextItemDataList &data) {
    bool different = (m_textItems.size() != data.size());
    if (!different)
    {
        const auto it = std::find_if(data.begin(), data.end(), [this](const QnOverlayTextItemData &textItemData) -> bool
        {
            const auto it = m_textItems.find(textItemData.id);
            return ((it == m_textItems.end()) || (*it != textItemData));
        });

        different = (it != data.end());
    }

    if (!different)
        return;

    clearTextItems();

    for (const auto textItemData: data)
        addTextItem(textItemData);
}

void QnTextOverlayWidget::clearTextItems()
{
    clear();
    m_textItems.clear();
    for (QTimer* timer: m_delayedRemoveTimers) {
        timer->stop();
        timer->deleteLater();
    }
    m_delayedRemoveTimers.clear();
}


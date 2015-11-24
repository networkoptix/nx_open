#pragma once

#include <utils/common/uuid.h>

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/controls/html_text_item.h>

#include <utils/common/model_functions_fwd.h>

struct QnOverlayTextItemData
{
    enum { kInfinite = -1 };

    QnUuid id;
    QString text;
    QnHtmlTextItemOptions itemOptions;
    int timeout;

    QnOverlayTextItemData(const QnUuid &initId
        , const QString &initText
        , QnHtmlTextItemOptions initItemOptions
        , int initTimeout = kInfinite);
};

QN_FUSION_DECLARE_FUNCTIONS(QnOverlayTextItemData, (eq));

typedef QList<QnOverlayTextItemData> QnOverlayTextItemDataList;

class QnTextOverlayWidgetPrivate;
class QnTextOverlayWidget : public GraphicsWidget 
{
public:
    QnTextOverlayWidget(QGraphicsWidget *parent = nullptr);

    ~QnTextOverlayWidget();

    void clear();

    void addItem(const QnOverlayTextItemData &data);

    void removeItem(const QnUuid &id);

    void setItems(const QnOverlayTextItemDataList &data);

private:
    Q_DECLARE_PRIVATE(QnTextOverlayWidget)
    QScopedPointer<QnTextOverlayWidgetPrivate> d_ptr;
};

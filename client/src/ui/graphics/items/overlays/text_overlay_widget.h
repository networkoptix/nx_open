#pragma once

#include <utils/common/uuid.h>

#include <core/resource/resource_fwd.h>

#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/controls/html_text_item.h>

struct QnOverlayTextItemData
{
    enum { kNoTimeout = -1 };

    QnUuid id;
    QString text;
    QnHtmlTextItemOptions itemOptions;
    int timeout;
    qint64 innerTimestamp;      /// Used to filter outdated

    QnOverlayTextItemData(const QnUuid &initId
        , const QString &initText
        , QnHtmlTextItemOptions initItemOptions
        , int initTimeout = kNoTimeout);

    bool operator == (const QnOverlayTextItemData &rhs) const;

    bool operator != (const QnOverlayTextItemData &rhs) const;
};

typedef QList<QnOverlayTextItemData> QnOverlayTextItemDataList;

class QnTextOverlayWidgetPrivate;
class QnTextOverlayWidget : public GraphicsWidget 
{
public:
    QnTextOverlayWidget(QGraphicsWidget *parent = nullptr);

    ~QnTextOverlayWidget();

    void resetItemsData();

    void addItemData(const QnOverlayTextItemData &data);

    void removeItemData(const QnUuid &id);

    void setItemsData(const QnOverlayTextItemDataList &data);

private:
    Q_DECLARE_PRIVATE(QnTextOverlayWidget)
    QScopedPointer<QnTextOverlayWidgetPrivate> d_ptr;
};

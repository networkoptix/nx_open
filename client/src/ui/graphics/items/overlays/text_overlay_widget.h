#pragma once

#include <nx/utils/uuid.h>

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

#define QnOverlayTextItemData_Fields (id)(text)(itemOptions)(timeout)

typedef QList<QnOverlayTextItemData> QnOverlayTextItemDataList;

class QnTextOverlayWidgetPrivate;

/**
 *  Container class, that displays QnHtmlTextItems on the scrollable area.
 *  Supports items auto-removing by timeout.
 */
class QnTextOverlayWidget : public GraphicsWidget
    //TODO: #GDM refactor to normal logic:
    // * replace class with QnScrollableOverlayWidget
    // * move 'remove-by-timeout' logic to controller class
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

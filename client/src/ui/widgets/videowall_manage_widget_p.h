#ifndef VIDEOWALL_MANAGE_WIDGET_PRIVATE_H
#define VIDEOWALL_MANAGE_WIDGET_PRIVATE_H

#include <QtCore/QRect>

#include <QtGui/QTransform>

#include <client/client_model_types.h>

#include <core/resource/resource_fwd.h>
#include <core/misc/screen_snap.h>

#include <ui/common/geometry.h>

#include <ui/processors/drag_process_handler.h>


enum class ItemType {
    Existing,
    ScreenPart,
    Screen,
    Added
};

enum class StateFlag {
    Default = 0,        /**< Default button state. */
    Pressed = 0x2,      /**< Button is pressed. This is the state that the button enters when a mouse button is pressed over it, and leaves when the mouse button is released. */
    Hovered = 0x4,      /**< Button is hovered over. */
    Disabled = 0x8     /**< Button is disabled. */
};
Q_DECLARE_FLAGS(StateFlags, StateFlag)

struct BaseModelItem {
    BaseModelItem(const QRect &rect);

    QUuid id;
    QRect geometry;
    ItemType itemType;
    QnScreenSnaps snaps;
};


struct ModelItem: BaseModelItem {
    ModelItem();
};

struct ModelScreenPart: BaseModelItem {
    ModelScreenPart(int screenIdx, int xIndex, int yIndex, const QRect &rect);

    bool free;
};

struct ModelScreen: BaseModelItem {
    ModelScreen(int idx, const QRect &rect);

    bool free() const;
    void setFree(bool value);

    QList<ModelScreenPart> parts;
};

class QnVideowallManageWidgetPrivate : private QnGeometry {
    Q_DECLARE_TR_FUNCTIONS(QnVideowallManageWidgetPrivate);
public:
    QnVideowallManageWidgetPrivate();

    QTransform getTransform(const QRect &rect);
    QTransform getInvertedTransform(const QRect &rect);
    QRect targetRect(const QRect &rect) const;
    QUuid itemIdByPos(const QPoint &pos);
    void addItemTo(const QUuid &id);
    void use(const QnScreenSnaps &snaps);

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall);

    void paint(QPainter* painter, const QRect &rect);
private:
    QRect unitedGeometry;
    QList<ModelItem> items;
    QList<ModelScreen> screens;
    QList<ModelItem> added;

private:
    void paintScreenFrame(QPainter *painter, const BaseModelItem &item);
    void paintPlaceholder(QPainter* painter, const BaseModelItem &item);
    void paintExistingItem(QPainter* painter, const BaseModelItem &item);
    void paintAddedItem(QPainter* painter, const BaseModelItem &item);

private:
    QTransform m_transform;
    QTransform m_invertedTransform;
    QRect m_widgetRect;
};

#endif // VIDEOWALL_MANAGE_WIDGET_PRIVATE_H

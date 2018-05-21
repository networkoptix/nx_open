#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace QnNotificationLevel { enum class Value; }

namespace nx {
namespace client {
namespace desktop {

class EventListModel;
class EventRibbon;
class EventTile;

class NotificationListWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    NotificationListWidget(QWidget* parent = nullptr);
    virtual ~NotificationListWidget() override;

signals:
    void unreadCountChanged(int count, QnNotificationLevel::Value importance);
    void tileHovered(const QModelIndex& index, const EventTile* tile);

private:
    // We capture mouse press to cancel global context menu.
    virtual void mousePressEvent(QMouseEvent *event) override;

    class Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx

#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::client::desktop {

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
    void tileHovered(const QModelIndex& index, EventTile* tile);

private:
    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

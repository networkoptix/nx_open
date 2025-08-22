// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/event/level.h>

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
    void unreadCountChanged(int count, nx::vms::event::Level importance);
    void tileHovered(const QModelIndex& index, EventTile* tile);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

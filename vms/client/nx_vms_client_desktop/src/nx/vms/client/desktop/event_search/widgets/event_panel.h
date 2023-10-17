// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/window_context_aware.h>

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::client::desktop {

class EventTile;

/**
 * Contains tabs and unread counter. Handles tabs visibility depending on the access rights and the
 * current context. Also manages global panel context menu (not per-tile).
 */
class EventPanel:
    public QWidget,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit EventPanel(WindowContext* context, QWidget* parent = nullptr);

    virtual ~EventPanel() override;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Tab,
        notifications,
        motion,
        bookmarks,
        events,
        vmsEvents,
        analytics
    )
    Q_ENUM(Tab)

signals:
    void unreadCountChanged(int count, QnNotificationLevel::Value importance);
    void currentTabChanged(Tab current, QPrivateSignal);

private:
    // We capture mouse press to cancel global context menu.
    virtual void mousePressEvent(QMouseEvent* event) override;

    class Private;
    nx::utils::ImplPtr<Private> d;
};
} // namespace nx::vms::client::desktop

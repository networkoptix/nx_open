#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/impl_ptr.h>

namespace QnNotificationLevel { enum class Value; }

namespace nx::vms::client::desktop {

class EventTile;

/**
 * Contains tabs and unread counter. Handles tabs visibility depending on the access rights and the
 * current context. Also manages global panel context menu (not per-tile).
 */
class EventPanel:
    public QWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QWidget;

public:
    explicit EventPanel(QWidget* parent);
    explicit EventPanel(QnWorkbenchContext* context, QWidget* parent = nullptr);

    virtual ~EventPanel() override;

    enum class Tab
    {
        notifications,
        motion,
        bookmarks,
        events,
        analytics
    };
    Q_ENUM(Tab);

    Tab currentTab() const;
    bool setCurrentTab(Tab tab);

signals:
    void unreadCountChanged(int count, QnNotificationLevel::Value importance);
    void tileHovered(const QModelIndex& index, EventTile* tile);
    void currentTabChanged(Tab current, QPrivateSignal);

private:
    // We capture mouse press to cancel global context menu.
    virtual void mousePressEvent(QMouseEvent *event) override;

    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

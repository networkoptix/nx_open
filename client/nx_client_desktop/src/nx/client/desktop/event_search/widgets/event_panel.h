#pragma once

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

namespace QnNotificationLevel { enum class Value; }

namespace nx::client::desktop {

class EventTile;

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

signals:
    void unreadCountChanged(int count, QnNotificationLevel::Value importance);
    void tileHovered(const QModelIndex& index, const EventTile* tile);

private:
    // We capture mouse press to cancel global context menu.
    virtual void mousePressEvent(QMouseEvent *event) override;

    class Private;
    QScopedPointer<Private> d;
};

} // namespace nx::client::desktop

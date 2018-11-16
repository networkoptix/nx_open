#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class EventRibbon;

class TileInteractionHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    static TileInteractionHandler* install(EventRibbon* ribbon);

private:
    TileInteractionHandler(EventRibbon* parent);

    void navigateToSource(const QModelIndex& index) const;
    void openSource(const QModelIndex& index, bool inNewTab) const;

private:
    const QPointer<EventRibbon> m_ribbon;
};

} // namespace nx::vms::client::desktop

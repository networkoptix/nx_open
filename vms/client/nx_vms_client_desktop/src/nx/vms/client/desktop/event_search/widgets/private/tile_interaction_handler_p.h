#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>

#include <ui/workbench/workbench_context_aware.h>

class QnGraphicsMessageBox;
namespace nx::utils { class PendingOperation; }

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

    void navigateToSource(const QModelIndex& index);
    void openSource(const QModelIndex& index, bool inNewTab);

    void showMessage(const QString& text);
    void showMessageDelayed(const QString& text, std::chrono::milliseconds delay);
    void hideMessages();

private:
    const QPointer<EventRibbon> m_ribbon;
    const QScopedPointer<nx::utils::PendingOperation> m_showPendingMessages;
    QSet<QnGraphicsMessageBox*> m_messages;
    QStringList m_pendingMessages;
};

} // namespace nx::vms::client::desktop

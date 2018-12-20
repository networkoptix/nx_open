#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtGui/QPixmap>

#include <core/resource/resource_fwd.h>
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
    virtual ~TileInteractionHandler() override;

    static TileInteractionHandler* install(EventRibbon* ribbon);

private:
    TileInteractionHandler(EventRibbon* parent);

    void navigateToSource(const QModelIndex& index);
    void openSource(const QModelIndex& index, bool inNewTab);
    void performDragAndDrop(const QModelIndex& index, const QPoint& pos, const QSize& size);

    void showMessage(const QString& text);
    void showMessageDelayed(const QString& text, std::chrono::milliseconds delay);
    void hideMessages();

    QPixmap createDragPixmap(const QnResourceList& resources, int width) const;

private:
    const QPointer<EventRibbon> m_ribbon;
    const QScopedPointer<nx::utils::PendingOperation> m_showPendingMessages;
    QSet<QnGraphicsMessageBox*> m_messages;
    QStringList m_pendingMessages;
};

} // namespace nx::vms::client::desktop

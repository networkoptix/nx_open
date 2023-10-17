// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QSet>
#include <QtGui/QPixmap>

#include <core/resource/resource_fwd.h>
#include <nx/utils/scope_guard.h>
#include <ui/workbench/workbench_context_aware.h>

class QJsonObject;
namespace nx::utils { class PendingOperation; }
namespace nx::analytics::db { struct ObjectTrack; }

namespace nx::vms::client::desktop {

class EventRibbon;
class RightPanelModelsAdapter;

class TileInteractionHandler:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    virtual ~TileInteractionHandler() override;

    static TileInteractionHandler* install(EventRibbon* ribbon);
    static TileInteractionHandler* install(RightPanelModelsAdapter* adapter);

private:
    TileInteractionHandler(WindowContext* context, QObject* parent = nullptr);

    void handleClick(
        const QModelIndex& index,
        Qt::MouseButton button,
        Qt::KeyboardModifiers modifiers);
    void handleDoubleClick(const QModelIndex& index);

    void navigateToSource(const QPersistentModelIndex& index, bool instantMessages);
    void openSource(const QModelIndex& index, bool inNewTab, bool fromDoubleClick);
    void performDragAndDrop(const QModelIndex& index, const QPoint& pos, const QSize& size);
    void showContextMenu(
        const QModelIndex& index,
        const QPoint& globalPos,
        bool withStandardInteraction,
        QWidget* parent);

    void showMessage(const QString& text);
    void showMessageDelayed(const QString& text, std::chrono::milliseconds delay);
    void hideMessages();

    QPixmap createDragPixmap(const QnResourceList& resources, int width) const;

    nx::utils::Guard scopedPlaybackStarter(bool baseCondition);

    QHash<int, QVariant> setupDropActionParameters(
        const QnResourceList& resources, const QVariant& timestamp) const;

    void executePluginAction(
        const QnUuid& engineId,
        const QString& actionTypeId,
        const nx::analytics::db::ObjectTrack& track,
        const QnVirtualCameraResourcePtr& camera) const;

    void copyBookmarkToClipboard(const QModelIndex& index);

    enum class ActionSupport
    {
        supported,
        unsupported,

        /** Generally supported, but not for the Cross-System tiles. */
        crossSystem,
        interactionRequired,
    };
    ActionSupport checkActionSupport(const QModelIndex& index);

signals:
    void highlightedTimestampChanged(std::chrono::microseconds value);
    void highlightedResourcesChanged(const QSet<QnResourcePtr>& value);

private:
    template<typename T>
    static TileInteractionHandler* doInstall(WindowContext* context, T* tileInteractionSource);

private:
    const QScopedPointer<nx::utils::PendingOperation> m_showPendingMessages;
    QSet<QnUuid> m_messages;
    QStringList m_pendingMessages;
};

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchLayout;
class QnWorkbenchItem;
class QGraphicsWidget;

namespace nx::utils { class PendingOperation; }

namespace nx::vms::api {
struct ShowreelData;
struct ShowreelItemData;
using ShowreelItemDataList = std::vector<ShowreelItemData>;
} // namespace nx::vms::api

namespace nx::vms::client::desktop {

class ShowreelDropPlaceholder;

class ShowreelReviewController: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ShowreelReviewController(QObject* parent = nullptr);
    virtual ~ShowreelReviewController() override;

private:
    void handleShowreelChanged(const nx::vms::api::ShowreelData& showreel);
    void handleShowreelRemoved(const nx::Uuid& showreelId);

    /** Handle current layout changes to update the Showreel review. */
    void startListeningLayout();

    /** Stop handling current layout changes. */
    void stopListeningLayout();

    void reviewShowreel(const nx::vms::api::ShowreelData& showreel);

    nx::Uuid currentShowreelId() const;
    bool isShowreelReviewMode() const;

    void connectToLayout(QnWorkbenchLayout* layout);

    void updateOrder();
    void updateButtons(const LayoutResourcePtr& layout);
    void updatePlaceholders();
    void updateItemsLayout();

    void resetReviewLayout(const LayoutResourcePtr& layout,
        const nx::vms::api::ShowreelItemDataList& items);

    void addItemToReviewLayout(
        const LayoutResourcePtr& layout,
        const nx::vms::api::ShowreelItemData& item,
        const QPointF& position,
        bool pinItem);
    void addResourcesToReviewLayout(
        const LayoutResourcePtr& layout,
        const QnResourceList& resources,
        const QPointF& position);

    /** Calculate items from the review layout. */
    bool fillShowreelItems(nx::vms::api::ShowreelItemDataList* items);

    void handleItemDataChanged(const nx::Uuid& id, int role, const QVariant& data);

// Actions handlers
private:
    void at_reviewShowreelAction_triggered();
    void at_dropResourcesAction_triggered();
    void at_startCurrentShowreelAction_triggered();
    void at_saveCurrentShowreelAction_triggered();
    void at_removeCurrentShowreelAction_triggered();

private:
    nx::utils::ScopedConnections m_connections;
    QHash<nx::Uuid, LayoutResourcePtr> m_reviewLayouts;
    QHash<QPoint, QSharedPointer<ShowreelDropPlaceholder>> m_dropPlaceholders;
    QSet<nx::Uuid> m_saveShowreelsQueue;
    nx::utils::PendingOperation* m_saveShowreelsOperation = nullptr;
    nx::utils::PendingOperation* m_updateItemsLayoutOperation = nullptr;
    bool m_updating = false;
};

} // namespace nx::vms::client::desktop

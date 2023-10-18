// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/node_view/details/node_view_fwd.h>

#include "poe_settings_state.h"

class QnResourcePool;
namespace nx::vms::api { struct NetworkBlockData; };

namespace nx::vms::client::desktop {

namespace node_view::details { class NodeViewStore; }

class PoeSettingsStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    PoeSettingsStore(
        QnResourcePool* resourcePool,
        QObject* parent = nullptr);

    void setStores(
        const node_view::details::NodeViewStorePtr& blockTableStore,
        const node_view::details::NodeViewStorePtr& totalsTableStore);

    virtual ~PoeSettingsStore() override;

    const PoeSettingsState& state() const;

    void updateBlocks(const nx::vms::api::NetworkBlockData& data);

    void setHasChanges(bool value);

    void setBlockUi(bool value);

    void setPreloaderVisible(bool value);

    void setAutoUpdates(bool value);

    void applyUserChanges();

    void resetUserChanges();

signals:
    void patchApplied(const PoeSettingsStatePatch& patch);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

#pragma once

#include "poe_settings_state.h"

#include <QtCore/QObject>

#include <nx/vms/client/desktop/node_view/details/node_view_fwd.h>
#include <nx/utils/impl_ptr.h>

class QnCommonModuleAware;
namespace nx::vms::api { class NetworkBlockData; };

namespace nx::vms::client::desktop {

namespace node_view::details { class NodeViewStore; }

namespace settings {

class PoESettingsStore: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    PoESettingsStore(
        QnCommonModuleAware* commonModuleAware,
        QObject* parent = nullptr);

    void setStores(
        const node_view::details::NodeViewStorePtr& blockTableStore,
        const node_view::details::NodeViewStorePtr& totalsTableStore);

    virtual ~PoESettingsStore() override;

    const PoESettingsState& state() const;

    void updateBlocks(const nx::vms::api::NetworkBlockData& data);

    void setHasChanges(bool value);

    void setBlockUi(bool value);

    void applyUserChanges();

signals:
    void patchApplied(const PoESettingsStatePatch& patch);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace settings
} // namespace nx::vms::client::desktop

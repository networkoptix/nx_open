// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::core::analytics::taxonomy { class StateView; }

namespace nx::vms::client::desktop {
class LookupListModel;

/**
 * Dialog, which allows to edit existing Lookup List Model ( if it passed as sourceModel)
 * or create a new one.
 */
class LookupListEditDialog: public QmlDialogWrapper, public SystemContextAware
{
    Q_OBJECT
    using base_type = QmlDialogWrapper;

public:
    static void registerQmlTypes();

    LookupListEditDialog(SystemContext* systemContext,
        core::analytics::taxonomy::StateView* taxonomy,
        LookupListModel* sourceModel, //< Model is used as source, but not edited in process.
        QWidget* parent = nullptr);
    virtual ~LookupListEditDialog() override;

    /** Result lookup list data, that is used during editing. */
    api::LookupListData getLookupListData();
};

} // namespace nx::vms::client::desktop

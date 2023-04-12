// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/common/dialogs/qml_dialog_wrapper.h>

namespace nx::vms::client::desktop {

class LookupListsDialog: public QmlDialogWrapper
{
    Q_OBJECT
    using base_type = QmlDialogWrapper;

public:
    LookupListsDialog(QWidget* parent = nullptr);
    virtual ~LookupListsDialog() override;

    void setData(nx::vms::api::LookupListDataList data);

    /** Show error text over the dialog. */
    void showError(const QString& text);

    /** Notify the dialog about saving result. */
    void setSaveResult(bool success);

    Q_INVOKABLE void requestSave();

signals:
    void saveRequested(nx::vms::api::LookupListDataList data);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
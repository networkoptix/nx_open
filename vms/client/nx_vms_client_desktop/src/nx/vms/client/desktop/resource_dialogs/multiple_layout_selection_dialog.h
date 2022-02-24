// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/utils/uuid.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui { class MultipleLayoutSelectionDialog; }

namespace nx::vms::client::desktop {

class MultipleLayoutSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    static bool selectLayouts(
        QnUuidSet& selectedLayouts,
        QWidget* parent);

private:
    MultipleLayoutSelectionDialog(
        const QnUuidSet& selectedLayouts,
        QWidget* parent = nullptr);

    virtual ~MultipleLayoutSelectionDialog() override;

private:
    struct Private;
    const std::unique_ptr<Private> d;
    const std::unique_ptr<Ui::MultipleLayoutSelectionDialog> ui;
};

} // namespace nx::vms::client::desktop


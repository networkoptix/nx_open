// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/software_version.h>

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui { class VersionSelectionDialog; }

namespace nx::vms::client::desktop {

class VersionSelectionDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit VersionSelectionDialog(QWidget* parent);
    virtual ~VersionSelectionDialog() override;

    nx::utils::SoftwareVersion version() const;
    QString password() const;

    virtual void accept() override;

private:
    QScopedPointer<Ui::VersionSelectionDialog> ui;
};

} // namespace nx::vms::client::desktop

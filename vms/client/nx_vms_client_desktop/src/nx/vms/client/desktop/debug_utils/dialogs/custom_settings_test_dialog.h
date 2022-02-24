// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

namespace Ui { class CustomSettingsTestDialog; }

namespace nx::vms::client::desktop {

class CustomSettingsTestDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    CustomSettingsTestDialog(QWidget* parent = nullptr);
    virtual ~CustomSettingsTestDialog() override;

    static void registerAction();
private:
    void loadManifest();

private:
    QScopedPointer<Ui::CustomSettingsTestDialog> ui;
};
} // namespace nx::vms::client::desktop

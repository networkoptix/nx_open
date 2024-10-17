// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop {

class PopupSettingsWidget;

class NotificationSettingsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit NotificationSettingsDialog(QWidget* parent);

    void accept() override;

protected:
    void showEvent(QShowEvent* event) override;

private:
    PopupSettingsWidget* m_settingsWidget;
};

} // namespace nx::vms::client::desktop

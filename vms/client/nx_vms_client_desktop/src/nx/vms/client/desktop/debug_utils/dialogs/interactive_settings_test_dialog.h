// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ui/dialogs/common/dialog.h>

class QQuickWidget;

namespace Ui { class InteractiveSettingsTestDialog; }

namespace nx::vms::client::desktop {

class InteractiveSettingsTestDialog: public QnDialog
{
    Q_OBJECT
    using base_type = QnDialog;

public:
    InteractiveSettingsTestDialog(QWidget* parent = nullptr);
    virtual ~InteractiveSettingsTestDialog() override;

    static void registerAction();

private:
    void loadManifest();

private slots:
    void refreshValues();

private:
    QScopedPointer<Ui::InteractiveSettingsTestDialog> ui;
    QQuickWidget* m_settingsWidget = nullptr;
};

} // namespace nx::vms::client::desktop

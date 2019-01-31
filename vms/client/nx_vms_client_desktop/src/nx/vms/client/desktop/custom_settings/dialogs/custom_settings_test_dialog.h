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

private:
    void loadManifest();

private:
    QScopedPointer<Ui::CustomSettingsTestDialog> ui;
};
} // namespace nx::vms::client::desktop

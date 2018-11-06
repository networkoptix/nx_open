#pragma once

#include <ui/dialogs/common/button_box_dialog.h>

#include <nx/utils/std/optional.h>

namespace Ui { class CameraCredentialsDialog; }

namespace nx::vms::client::desktop {

class CameraCredentialsDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit CameraCredentialsDialog(QWidget* parent = nullptr);
    virtual ~CameraCredentialsDialog() override;

    std::optional<QString> login() const;
    void setLogin(const std::optional<QString>& value);

    std::optional<QString> password() const;
    void setPassword(const std::optional<QString>& value);

private:
    const QScopedPointer<Ui::CameraCredentialsDialog> ui;
};

} // namespace nx::vms::client::desktop

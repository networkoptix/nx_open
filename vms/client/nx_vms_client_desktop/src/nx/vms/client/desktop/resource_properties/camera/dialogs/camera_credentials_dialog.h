// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    /**
     * @return Login value entered or set to the input field. std::nullopt returned means that
     *     login input field shows "Multiple values" placeholder.
     */
    std::optional<QString> login() const;

    /**
     * Sets value to the login input field. Login input field displays "Multiple values"
     * placeholder if std::nullopt is passed as parameter.
     */
    void setLogin(const std::optional<QString>& value);

    /**
     * @return Password value entered or set to the input field. std::nullopt returned has meaning
     *     that password input field shows "Multiple values" placeholder. In the "Remote password"
     *     state empty string is returned.
     */
    std::optional<QString> password() const;

    /**
     * Sets value to the password input field. Password input field displays "Multiple values"
     * placeholder if std::nullopt is passed as parameter. The dialog is no longer in the
     * "Remote password" state after this method call.
     */
    void setPassword(const std::optional<QString>& value);

    /**
     * @return Whether password input field in the state when it displays dots placeholder but
     *     actually doesn't hold any value. There are no "Eye" control in that state. Password
     *     input field is no longer in that state after any user input is done to the either login
     *     or password input field.
     */
    bool hasRemotePassword() const;

    /**
     * Sets password input field to the state when it displays dots placeholder but actually
     * doesn't hold any value.
     */
    void setHasRemotePassword(bool value);

private:
    const QScopedPointer<Ui::CameraCredentialsDialog> ui;
};

} // namespace nx::vms::client::desktop

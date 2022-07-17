// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "input_field.h"

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/utils/password_information.h>

namespace nx::vms::client::desktop {

/**
 * Common class for the password input fields. Can check input for validity and display warning
 * hint if something wrong. Stored password text which set using `setText()` method will be is
 * considered locked, so user won't be able to open it with the "eye" button, copy to clipboard or
 * change any part of the password except completely overwrite it. When user starts typing the
 * password, the stored one will be cleared and the input field will become unlocked.
 */
class PasswordInputField: public InputField
{
    Q_OBJECT
    using base_type = InputField;

public:
    PasswordInputField(QWidget* parent = nullptr);
    virtual ~PasswordInputField() override;

    virtual ValidationResult calculateValidationResult() const override;

    void setPasswordIndicatorEnabled(
        bool enabled,
        bool hideForEmptyInput = true,
        bool showImmediately = false,
        PasswordInformation::AnalyzeFunction analyzeFunction = nx::utils::passwordStrength);

    virtual QString text() const override;
    virtual void setText(const QString& value) override;

    // Whether the password is stored on the server and isn't accessible on the client.
    bool hasRemotePassword() const;
    void setHasRemotePassword(bool value);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

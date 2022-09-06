// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "input_field.h"

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/common/utils/password_information.h>

namespace nx::vms::client::desktop {

/**
 * Common class for the password input fields. Can check input for validity and display warning
 * hint if something wrong.
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

    /**
     * @return Value entered in the input field if <tt>hasRemotePassword()<tt> returns false, null
     *     string otherwise despite the fact that input field appears as non-empty.
     */
    virtual QString text() const override;

    /**
     * Sets given value to the input field. Remote password indication state resets after this call
     * regardless of the value passed.
     */
    virtual void setText(const QString& value) override;

    /**
     * Remote password indication state lets know if password is stored on the server but isn't
     * accessible on the client. In this state input field appears as non-empty, but it doesn't
     * provide any value. Respectively, no input value validation are made.
     *
     * The control changes to its normal state on any attempt to change the value of the input
     * field.
     * @return Whether control is in remote password indication state or not.
     */
    bool hasRemotePassword() const;

    /**
     * Toggles remote password indication state. The entered value is discarded upon enabling
     * this state.
     */
    void setHasRemotePassword(bool value);

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

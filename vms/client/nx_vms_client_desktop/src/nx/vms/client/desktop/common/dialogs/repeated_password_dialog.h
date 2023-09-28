// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class RepeatedPasswordDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

/**
 * Dialog where user can input password and should repeat it to enable "OK" button.
 * Explanatory text is shown at top part of the dialog, the text can be set via setHeaderText().
 * The dialog will be automatically closed on client disconnect.
 */
class RepeatedPasswordDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    RepeatedPasswordDialog(QWidget *parent);
    virtual ~RepeatedPasswordDialog() override;

    void setHeaderText(const QString& text);
    QString headerText() const;

    /**
     * @return Password entered and repeated by user or empty string if passwords did not match.
     */
    QString password() const;

    void clearInputFields();

private:
    bool passwordsMatch() const;
    void updateMatching();

private:
    QScopedPointer<Ui::RepeatedPasswordDialog> ui;
};

} // namespace nx::vms::client::desktop

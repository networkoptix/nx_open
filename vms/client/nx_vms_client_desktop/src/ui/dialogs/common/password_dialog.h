// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
class PasswordDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

/**
 * Username and password input dialog that will be automatically closed on client disconnect.
 */
class PasswordDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    PasswordDialog(QWidget* parent);
    virtual ~PasswordDialog() override;

    QString caption() const;
    void setCaption(const QString& caption);

    QString text() const;
    void setText(const QString& text);

    void setUsername(const QString& username, bool updateFocus = true);

    QString username() const;
    QString password() const;

private:
    QScopedPointer<Ui::PasswordDialog> ui;
};

} // namespace nx::vms::client::desktop

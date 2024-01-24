// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QByteArray;

namespace Ui {
class EncryptedArchivePasswordDialog;
} // namespace Ui

namespace nx::vms::client::desktop {

/**
 * Dialog where user can input password to decrypt previously encrypted archive in case if
 * server lost the password somehow.
 * The only valid way to show is to call showForEncryptionData() method.
 * Checks password for validity on client side and sends valid password to server upon Decrypt
 * button pressed.
 * accepted() signal emitted after setting password request responce received from server
 * (regardless of successfull or not).
 * rejected() signal emitted upon pressing Cancel button by user only.
 * The dialog will be automatically closed on client disconnect.
 */
class EncryptedArchivePasswordDialog:
    public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    EncryptedArchivePasswordDialog(const QnMediaResourcePtr& mediaResource, QWidget* parent);
    ~EncryptedArchivePasswordDialog();

    /**
     * The only valid way to show the dialog.
     * @param ivVect kind of hash of password used to encrypt the archive.
     *     Received from server with MediaStreamEvent::cannotDecryptMedia.
     */
    void showForEncryptionData(const QByteArray& encryptionData);

private:
    QScopedPointer<Ui::EncryptedArchivePasswordDialog> ui;
};

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "force_secure_auth_dialog.h"

#include <QtWidgets/QPushButton>

#include <client/client_show_once_settings.h>

namespace nx::vms::client::desktop {

namespace {

const QString kDigestDisableShowOnceKey = "DigestDisableNotification";

} // namespace

ForceSecureAuthDialog::ForceSecureAuthDialog(QWidget* parent):
    base_type(parent)
{
    setIcon(QnMessageBoxIcon::Question);
    setText(tr("Force secure authentication?"));
    setInformativeText(
        tr("To revert this change user password reset will be required."),
        /*split*/ true);

    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    setDefaultButton(QDialogButtonBox::Ok);
    setCustomCheckBoxText(tr("Do not show this message again"));
    button(QDialogButtonBox::Ok)->setText(tr("Enable"));
}

bool ForceSecureAuthDialog::isConfirmed(QWidget* parent)
{
    if (qnClientShowOnce->testFlag(kDigestDisableShowOnceKey))
        return true;

    ForceSecureAuthDialog dialog(parent);
    if (dialog.exec() == QDialogButtonBox::Cancel)
        return false;

    if (dialog.isChecked())
        qnClientShowOnce->setFlag(kDigestDisableShowOnceKey, true);

    return true;
}

} // nx::vms::client::desktop

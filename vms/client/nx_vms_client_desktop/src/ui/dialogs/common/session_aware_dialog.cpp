// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_aware_dialog.h"

/************************************************************************/
/* QnSessionAwareButtonBoxDialog                             */
/************************************************************************/
QnSessionAwareButtonBoxDialog::QnSessionAwareButtonBoxDialog(
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    QnSessionAwareDelegate(parent)
{
}

bool QnSessionAwareButtonBoxDialog::tryClose(bool force)
{
    base_type::reject();
    if (force)
        base_type::hide();
    return true;
}

void QnSessionAwareButtonBoxDialog::forcedUpdate()
{
    retranslateUi();
    tryClose(true);
}

/************************************************************************/
/* QnSessionAwareTabbedDialog                                */
/************************************************************************/
QnSessionAwareTabbedDialog::QnSessionAwareTabbedDialog(
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    QnSessionAwareDelegate(parent)
{
}

bool QnSessionAwareTabbedDialog::tryClose(bool force)
{
    if (force)
    {
        forcefullyClose();
        return true;
    }

    if (!tryToApplyOrDiscardChanges())
        return false;

    hide();
    return true;
}

bool QnSessionAwareTabbedDialog::tryToApplyOrDiscardChanges()
{
    if (!canDiscardChanges())
        return false;

    if (isHidden() || !hasChanges())
        return true;

    switch (showConfirmationDialog())
    {
        case QDialogButtonBox::Yes:
        case QDialogButtonBox::Apply:
            if (!canApplyChanges())
                return false;
            applyChanges();
            break;

        case QDialogButtonBox::No:
        case QDialogButtonBox::Discard:
            loadDataToUi();
            break;

        default:
            return false; //< Cancel was pressed.
    }

    return true;
}

void QnSessionAwareTabbedDialog::forcedUpdate()
{
    loadDataToUi();
}

QDialogButtonBox::StandardButton QnSessionAwareTabbedDialog::showConfirmationDialog()
{
    QStringList details;
    for (const Page& page: modifiedPages())
        details << "* " + page.title;

    static const auto kDelimiter = '\n';
    const auto extraMessage = tr("Unsaved changes:") + kDelimiter + details.join(kDelimiter);

    return QnMessageBox::question(
        this,
        tr("Save changes before exit?"),
        extraMessage,
        QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel,
        QDialogButtonBox::Yes);
}

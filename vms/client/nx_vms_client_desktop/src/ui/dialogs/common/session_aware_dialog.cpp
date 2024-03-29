// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "session_aware_dialog.h"

#include <QtWidgets/QPushButton>

#include <nx/vms/client/desktop/window_context.h>

using namespace nx::vms::client::desktop;

//-------------------------------------------------------------------------------------------------
// QnSessionAwareButtonBoxDialog

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

//-------------------------------------------------------------------------------------------------
// QnSessionAwareTabbedDialog

QnSessionAwareTabbedDialog::QnSessionAwareTabbedDialog(
    QWidget* parent,
    Qt::WindowFlags windowFlags)
    :
    base_type(parent, windowFlags),
    QnSessionAwareDelegate(parent)
{
    connect(windowContext(), &WindowContext::systemChanged, this,
        &QnSessionAwareTabbedDialog::loadDataToUi);
}

bool QnSessionAwareTabbedDialog::tryClose(bool force)
{
    if (force)
    {
        reject();
        return true;
    }

    return closeDialogWithConfirmation();
}

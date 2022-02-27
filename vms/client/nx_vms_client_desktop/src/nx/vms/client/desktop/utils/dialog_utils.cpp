// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "dialog_utils.h"

#include <QtWidgets/QDialog>

namespace nx::vms::client::desktop {

void DialogUtils::setDialogParent(QDialog* dialog, QWidget* parent)
{
    dialog->setParent(parent, dialog->windowFlags());
}

} // namespace nx::vms::client::desktop

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "name_value_table_test_dialog.h"

#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

NameValueTableTestDialog::NameValueTableTestDialog(QWidget* parent):
    base_type(
        QUrl("Nx/Dialogs/Test/NameValueTableTestDialog.qml"), /*initialProperties*/ {}, parent)
{
}

void NameValueTableTestDialog::registerAction()
{
    registerDebugAction(
        "Name-Value Table Test",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<NameValueTableTestDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop

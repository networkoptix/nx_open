// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "sorting_test_dialog.h"

#include <memory>

#include <QtCore/QUrl>

#include <ui/workbench/workbench_context.h>

#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

SortingTestDialog::SortingTestDialog(QWidget* parent): QmlDialogWrapper(
    QUrl("Nx/Dialogs/Test/SortingTestDialog.qml"),
    /*initialProperties*/ {},
    parent)
{
}

void SortingTestDialog::registerAction()
{
    registerDebugAction(
        "Sorting Test",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<SortingTestDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop

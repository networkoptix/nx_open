// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "palette_dialog.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QBoxLayout>

#include <ui/workbench/workbench_context.h>

#include "../widgets/palette_widget.h"
#include "../utils/debug_custom_actions.h"

namespace nx::vms::client::desktop {

PaletteDialog::PaletteDialog(QWidget* parent):
    base_type(parent)
{
    auto paletteWidget = new PaletteWidget(this);
    paletteWidget->setPalette(qApp->palette());

    auto layout = new QVBoxLayout(this);
    layout->addWidget(paletteWidget);
}

void PaletteDialog::registerAction()
{
    registerDebugAction(
        "Palette",
        [](QnWorkbenchContext* context)
        {
            auto dialog = std::make_unique<PaletteDialog>(context->mainWindowWidget());
            dialog->exec();
        });
}

} // namespace nx::vms::client::desktop

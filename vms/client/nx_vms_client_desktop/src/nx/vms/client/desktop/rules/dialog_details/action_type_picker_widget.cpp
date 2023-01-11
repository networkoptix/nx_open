// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_type_picker_widget.h"

#include "ui_action_type_picker_widget.h"

#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

ActionTypePickerWidget::ActionTypePickerWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::ActionTypePickerWidget())
{
    ui->setupUi(this);
    setPaletteColor(ui->doLabel, QPalette::WindowText, colorTheme()->color("light1"));

    connect(ui->actionTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]()
        {
            emit actionTypePicked(actionType());
        });
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
ActionTypePickerWidget::~ActionTypePickerWidget()
{
}

void ActionTypePickerWidget::init(nx::vms::rules::Engine* engine)
{
    ui->actionTypeComboBox->clear();
    for (const auto& actionType: engine->actions())
        ui->actionTypeComboBox->addItem(actionType.displayName, actionType.id);
}

QString ActionTypePickerWidget::actionType() const
{
    return ui->actionTypeComboBox->currentData().toString();
}

void ActionTypePickerWidget::setActionType(const QString& actionType)
{
    ui->actionTypeComboBox->setCurrentIndex(ui->actionTypeComboBox->findData(actionType));
}

} // namespace nx::vms::client::desktop::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type_picker_widget.h"
#include "ui_event_type_picker_widget.h"

#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <nx/vms/rules/strings.h>
#include <nx/vms/rules/utils/common.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

EventTypePickerWidget::EventTypePickerWidget(SystemContext* context, QWidget* parent):
    QWidget(parent),
    SystemContextAware{context},
    ui(new Ui::EventTypePickerWidget())
{
    ui->setupUi(this);
    setPaletteColor(ui->whenLabel, QPalette::WindowText, QPalette().color(QPalette::Light));

    const auto servers = systemContext()->resourcePool()->servers();
    for (const auto& eventDescriptor: systemContext()->vmsRulesEngine()->events())
    {
        if (!vms::rules::utils::hasItemSupportedServer(servers, eventDescriptor))
            continue;

        if (eventDescriptor.flags.testFlag(vms::rules::ItemFlag::system))
            continue;

        ui->eventTypeComboBox->addItem(eventDescriptor.displayName, eventDescriptor.id);
    }

    connect(ui->eventTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]()
        {
            emit eventTypePicked(eventType());
        });
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
EventTypePickerWidget::~EventTypePickerWidget()
{
}

QString EventTypePickerWidget::eventType() const
{
    return ui->eventTypeComboBox->currentData().toString();
}

void EventTypePickerWidget::setEventType(const QString& eventType)
{
    if (auto index = ui->eventTypeComboBox->findData(eventType); index != -1)
    {
        ui->eventTypeComboBox->setCurrentIndex(index);
        return;
    }

    ui->eventTypeComboBox->setPlaceholderText(
        vms::rules::Strings::eventName(systemContext(), eventType));
    ui->eventTypeComboBox->setCurrentIndex(-1);
}

} // namespace nx::vms::client::desktop::rules

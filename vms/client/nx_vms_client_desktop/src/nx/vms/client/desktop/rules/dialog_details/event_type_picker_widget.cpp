// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type_picker_widget.h"

#include "ui_event_type_picker_widget.h"

#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/manifest.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop::rules {

EventTypePickerWidget::EventTypePickerWidget(QWidget* parent):
    QWidget(parent),
    ui(new Ui::EventTypePickerWidget())
{
    ui->setupUi(this);
    setPaletteColor(ui->whenLabel, QPalette::WindowText, QPalette().color(QPalette::Light));

    connect(ui->eventTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]()
        {
            emit eventTypePicked(eventType());
        });

    connect(ui->continuanceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        [this]()
        {
            emit eventContinuancePicked(eventContinuance());
        });

    std::list<std::pair<QString, QVariant>> continuanceList {
        {tr("None"), QVariant::fromValue(api::rules::State::none)},
        {tr("Starts"), QVariant::fromValue(api::rules::State::started)},
        {tr("Stops"), QVariant::fromValue(api::rules::State::stopped)},
        {tr("Instant"), QVariant::fromValue(api::rules::State::instant)}
    };

    ui->continuanceComboBox->clear();
    for (const auto& item: continuanceList)
        ui->continuanceComboBox->addItem(item.first, item.second);

    ui->continuanceComboBox->setCurrentIndex(3);
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
EventTypePickerWidget::~EventTypePickerWidget()
{
}

void EventTypePickerWidget::init(const nx::vms::rules::Engine* engine)
{
    ui->eventTypeComboBox->clear();
    for (const auto& eventType: engine->events())
        ui->eventTypeComboBox->addItem(eventType.displayName, eventType.id);
}

QString EventTypePickerWidget::eventType() const
{
    return ui->eventTypeComboBox->currentData().toString();
}

void EventTypePickerWidget::setEventType(const QString& eventType)
{
    ui->eventTypeComboBox->setCurrentIndex(ui->eventTypeComboBox->findData(eventType));
}

api::rules::State EventTypePickerWidget::eventContinuance() const
{
    return ui->continuanceComboBox->currentData().value<api::rules::State>();
}

} // namespace nx::vms::client::desktop::rules

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_text_overlay_action_widget.h"
#include "ui_show_text_overlay_action_widget.h"

#include <QtCore/QScopedValueRollback>

#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

using namespace nx;
using namespace nx::vms::client::desktop;

namespace {

    enum {
        msecPerSecond = 1000
    };
}

QString QnShowTextOverlayActionWidget::getPlaceholderText()
{
    static const auto kPlaceholderText = tr("Html tags could be used within custom text:\n<h4>Headers (h1-h6)</h4>Also different <font color=\"red\">colors</font> and <font size=\"18\">sizes</font> could be applied. Text could be <s>stricken</s>, <u>underlined</u>, <b>bold</b> or <i>italic</i>"
        , "Do not translate tags (text between '<' and '>' symbols. Do not remove '\n' sequence");

    return kPlaceholderText;
}

QnShowTextOverlayActionWidget::QnShowTextOverlayActionWidget(SystemContext* systemContext,
    QWidget* parent)
    :
    base_type(systemContext, parent),
    ui(new Ui::ShowTextOverlayActionWidget)
{
    ui->setupUi(this);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, ui->durationWidget, &QWidget::setEnabled);

    connect(ui->durationSpinBox,        QnSpinboxIntValueChanged,       this, &QnShowTextOverlayActionWidget::paramsChanged);
    connect(ui->customTextEdit,         &QPlainTextEdit::textChanged,   this, &QnShowTextOverlayActionWidget::paramsChanged);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::clicked, this, [this]()
    {
        const bool isFixedDuration = ui->fixedDurationCheckBox->isChecked();
        ui->ruleWarning->setVisible(!isFixedDuration);
        paramsChanged();
    });

    connect(ui->customTextCheckBox,     &QCheckBox::clicked, this, [this]()
    {
        const bool useCustomText = ui->customTextCheckBox->isChecked();

        if (!useCustomText)
        {
            /// Previous state is "use custom text", so update temporary holder
            m_lastCustomText = ui->customTextEdit->toPlainText();
        }

        ui->customTextEdit->setPlainText(useCustomText
            ? m_lastCustomText : getPlaceholderText());

        paramsChanged();
    });

    ui->customTextEdit->setEnabled(ui->customTextCheckBox->isChecked());
    connect(ui->customTextCheckBox, &QCheckBox::toggled, ui->customTextEdit, &QWidget::setEnabled);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, this, [this](bool checked)
    {
        // Prolonged type of event has changed. In case of instant
        // action event state should be updated
        if (checked && (model()->eventType() == vms::api::EventType::userDefinedEvent))
            model()->setEventState(vms::api::EventState::undefined);
    });
}

QnShowTextOverlayActionWidget::~QnShowTextOverlayActionWidget()
{}

void QnShowTextOverlayActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->durationSpinBox);
    setTabOrder(ui->durationSpinBox, ui->customTextCheckBox);
    setTabOrder(ui->customTextCheckBox, ui->customTextEdit);
    setTabOrder(ui->customTextEdit, after);
}

void QnShowTextOverlayActionWidget::at_model_dataChanged(Fields fields) {
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields.testFlag(Field::eventType))
    {
        bool hasToggleState = vms::event::hasToggleState(
            model()->eventType(), model()->eventParams(), systemContext());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);

        const bool canUseSource = ((model()->eventType() >= vms::api::EventType::userDefinedEvent)
            || (vms::event::requiresCameraResource(model()->eventType())));
    }

    if (fields.testFlag(Field::actionParams)) {
        const auto params = model()->actionParams();

        const bool isFixedFuration = (params.durationMs > 0);
        ui->fixedDurationCheckBox->setChecked(isFixedFuration);
        ui->ruleWarning->setVisible(!isFixedFuration);
        if (ui->fixedDurationCheckBox->isChecked())
            ui->durationSpinBox->setValue(params.durationMs / msecPerSecond);

        const bool useCustomText = !params.text.isEmpty();
        m_lastCustomText = params.text;
        ui->customTextCheckBox->setChecked(useCustomText);
        ui->customTextEdit->setPlainText(useCustomText ? params.text : getPlaceholderText());
    }
}

void QnShowTextOverlayActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    vms::event::ActionParameters params = model()->actionParams();

    params.durationMs = ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0;
    params.text = ui->customTextCheckBox->isChecked() ? ui->customTextEdit->toPlainText() : QString();

    model()->setActionParams(params);
}

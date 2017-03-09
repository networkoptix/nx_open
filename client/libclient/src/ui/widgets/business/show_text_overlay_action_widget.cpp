#include "show_text_overlay_action_widget.h"
#include "ui_show_text_overlay_action_widget.h"

#include <business/business_action_parameters.h>

#include <utils/common/scoped_value_rollback.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>

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

QnShowTextOverlayActionWidget::QnShowTextOverlayActionWidget(QWidget *parent)
    : base_type(parent)
    , ui(new Ui::ShowTextOverlayActionWidget)
    , m_lastCustomText()
{
    ui->setupUi(this);

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, ui->durationWidget, &QWidget::setEnabled);

    connect(ui->useSourceCheckBox,      &QCheckBox::clicked,            this, &QnShowTextOverlayActionWidget::paramsChanged);
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

        ui->customTextEdit->setEnabled(useCustomText);
        paramsChanged();
    });

    connect(ui->fixedDurationCheckBox,  &QCheckBox::toggled, this, [this](bool checked)
    {
        // Prolonged type of event has changed. In case of instant
        // action event state should be updated
        if (checked && (model()->eventType() == QnBusiness::UserDefinedEvent))
            model()->setEventState(QnBusiness::UndefinedState);
    });
}

QnShowTextOverlayActionWidget::~QnShowTextOverlayActionWidget()
{}

void QnShowTextOverlayActionWidget::updateTabOrder(QWidget *before, QWidget *after) {
    setTabOrder(before, ui->useSourceCheckBox);
    setTabOrder(ui->useSourceCheckBox, ui->fixedDurationCheckBox);
    setTabOrder(ui->fixedDurationCheckBox, ui->durationSpinBox);
    setTabOrder(ui->durationSpinBox, ui->customTextCheckBox);
    setTabOrder(ui->customTextCheckBox, ui->customTextEdit);
    setTabOrder(ui->customTextEdit, after);
}

void QnShowTextOverlayActionWidget::at_model_dataChanged(QnBusiness::Fields fields) {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields.testFlag(QnBusiness::EventTypeField))
    {
        bool hasToggleState = QnBusiness::hasToggleState(model()->eventType());
        if (!hasToggleState)
            ui->fixedDurationCheckBox->setChecked(true);
        setReadOnly(ui->fixedDurationCheckBox, !hasToggleState);

        const bool canUseSource = ((model()->eventType() >= QnBusiness::UserDefinedEvent)
            || (requiresCameraResource(model()->eventType())));
        ui->useSourceCheckBox->setEnabled(canUseSource);
    }

    if (fields.testFlag(QnBusiness::ActionParamsField)) {
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

        ui->useSourceCheckBox->setChecked(params.useSource);
    }
}



void QnShowTextOverlayActionWidget::paramsChanged() {
    if (!model() || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnBusinessActionParameters params = model()->actionParams();

    params.durationMs = ui->fixedDurationCheckBox->isChecked() ? ui->durationSpinBox->value() * msecPerSecond : 0;
    params.text = ui->customTextCheckBox->isChecked() ? ui->customTextEdit->toPlainText() : QString();
    params.useSource = ui->useSourceCheckBox->isChecked();
    model()->setActionParams(params);
}

#include "adjust_video_dialog.h"
#include "ui_adjust_video_dialog.h"

QnAdjustVideoDialog::QnAdjustVideoDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::AdjustVideoDialog),
    m_updateDisabled(false)
{
    ui->setupUi(this);

    connect(ui->gammaSlider, SIGNAL(valueChanged(int)), this, SLOT(at_sliderValueChanged()));
    connect(ui->blackLevelsSlider, SIGNAL(valueChanged(int)), this, SLOT(at_sliderValueChanged()));
    connect(ui->whiteLevelsSlider, SIGNAL(valueChanged(int)), this, SLOT(at_sliderValueChanged()));

    connect(ui->gammaSpinBox, SIGNAL(valueChanged(double)), this, SLOT(at_spinboxValueChanged()));
    connect(ui->blackLevelsSpinBox, SIGNAL(valueChanged(double)), this, SLOT(at_spinboxValueChanged()));
    connect(ui->whiteLevelsSpinBox, SIGNAL(valueChanged(double)), this, SLOT(at_spinboxValueChanged()));

    connect(ui->autoGammaCheckBox, SIGNAL(stateChanged(int)), this, SLOT(at_sliderValueChanged()));
    connect(ui->enableAdjustment, SIGNAL(toggled(bool)), this, SLOT(at_sliderValueChanged()));
    
    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(at_buttonClicked(QAbstractButton*)));
    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

QnAdjustVideoDialog::~QnAdjustVideoDialog() {
}


void QnAdjustVideoDialog::at_sliderValueChanged()
{
    if (m_updateDisabled)
        return;
    m_updateDisabled = true;

    ui->gammaSlider->setEnabled(!ui->autoGammaCheckBox->isChecked());
    ui->gammaSpinBox->setEnabled(!ui->autoGammaCheckBox->isChecked());
    if (ui->autoGammaCheckBox->isChecked())
        ui->gammaSlider->setValue(0);

    float c = ui->gammaSlider->value();
    ui->gammaSpinBox->setValue(1.0 + c/(c >= 0.0 ? 1000.0 : 10000.0));
    ui->blackLevelsSpinBox->setValue(ui->blackLevelsSlider->value() / (float) ui->blackLevelsSlider->maximum() * ui->blackLevelsSpinBox->maximum());
    ui->whiteLevelsSpinBox->setValue(ui->whiteLevelsSlider->value() / (float) ui->whiteLevelsSlider->maximum() * ui->whiteLevelsSpinBox->maximum());

    m_updateDisabled = false;
    uiToParams();
}

void QnAdjustVideoDialog::at_spinboxValueChanged()
{
    if (m_updateDisabled)
        return;
    m_updateDisabled = true;

    float c = ui->gammaSpinBox->value();
    ui->gammaSlider->setValue((c-1.0) * (c >= 1.0 ? 1000.0 : 10000.0));
    ui->blackLevelsSlider->setValue(ui->blackLevelsSpinBox->value() / (float) ui->blackLevelsSpinBox->maximum() * ui->blackLevelsSlider->maximum());
    ui->whiteLevelsSlider->setValue(ui->whiteLevelsSpinBox->value() / (float) ui->whiteLevelsSpinBox->maximum() * ui->whiteLevelsSlider->maximum());

    m_updateDisabled = false;
    uiToParams();
}

void QnAdjustVideoDialog::uiToParams()
{
    ImageCorrectionParams newParams;
    newParams.enabled = ui->enableAdjustment->isChecked();
    newParams.gamma = ui->autoGammaCheckBox->isChecked() ? 0.0 : ui->gammaSpinBox->value();
    newParams.blackLevel = ui->blackLevelsSpinBox->value() / 100.0;
    newParams.whiteLevel = 1.0 - ui->whiteLevelsSpinBox->value()/100.0;

    if (!(newParams == m_params)) {
        m_params = newParams;
        emit valueChanged(m_params);
    }
}

ImageCorrectionParams QnAdjustVideoDialog::params() const
{
    return m_params;
}

void QnAdjustVideoDialog::setParams(const ImageCorrectionParams& params)
{
    m_updateDisabled = true;

    ui->enableAdjustment->setChecked(params.enabled);
    ui->autoGammaCheckBox->setChecked(params.gamma == 0);
    ui->gammaSpinBox->setValue(params.gamma == 0 ? 1.0 : params.gamma);
    ui->blackLevelsSpinBox->setValue(params.blackLevel * 100.0);
    ui->whiteLevelsSpinBox->setValue(100.0 - params.whiteLevel * 100.0);

    ui->gammaSlider->setEnabled(params.gamma != 0);
    ui->gammaSpinBox->setEnabled(params.gamma != 0);

    m_updateDisabled = false;
    at_spinboxValueChanged();
}

void QnAdjustVideoDialog::at_buttonClicked(QAbstractButton* button)
{
    QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);
    if (role == QDialogButtonBox::ResetRole) {
        ImageCorrectionParams defaultParams;
        defaultParams.enabled = true;
        setParams(defaultParams);
    }
}

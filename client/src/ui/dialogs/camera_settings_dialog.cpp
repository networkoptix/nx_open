#include "camera_settings_dialog.h"

#include <QtGui/QVBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>

#include <ui/actions/action_manager.h>
#include <ui/widgets/properties/camera_settings_widget.h>
#include <ui/workbench/workbench_context.h>

QnCameraSettingsDialog::QnCameraSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    QDialog(parent, windowFlags)
{
    m_settingsWidget = new QnCameraSettingsWidget(this);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_applyButton = m_buttonBox->button(QDialogButtonBox::Apply);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_settingsWidget);
    layout->addWidget(m_buttonBox);

    connect(m_buttonBox,        SIGNAL(accepted()),                 this,   SLOT(accept()));
    connect(m_buttonBox,        SIGNAL(rejected()),                 this,   SLOT(reject()));
    connect(m_buttonBox,        SIGNAL(clicked(QAbstractButton *)), this,   SLOT(at_buttonBox_clicked(QAbstractButton *)));
    connect(m_settingsWidget,   SIGNAL(hasChangesChanged()),        this,   SLOT(at_settingsWidget_hasChangesChanged()));
    connect(m_settingsWidget,   SIGNAL(modeChanged()),              this,   SLOT(at_settingsWidget_modeChanged()));

    at_settingsWidget_hasChangesChanged();
}

QnCameraSettingsDialog::~QnCameraSettingsDialog() {
    return;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged() {
    m_applyButton->setEnabled(m_settingsWidget->hasDbChanges() || m_settingsWidget->hasCameraChanges());
}

void QnCameraSettingsDialog::at_settingsWidget_modeChanged() {
    m_okButton->setEnabled(m_settingsWidget->mode() == QnCameraSettingsWidget::SingleMode || m_settingsWidget->mode() == QnCameraSettingsWidget::MultiMode);
}

void QnCameraSettingsDialog::at_buttonBox_clicked(QAbstractButton *button) {
    emit buttonClicked(m_buttonBox->standardButton(button));
}


#include "camera_settings_dialog.h"

#include <QtGui/QVBoxLayout>
#include <QtGui/QDialogButtonBox>
#include <QtGui/QPushButton>

#include <ui/actions/action_manager.h>
#include <ui/widgets/properties/camera_settings_widget.h>
#include <ui/workbench/workbench_context.h>

QnCameraSettingsDialog::QnCameraSettingsDialog(QWidget *parent, Qt::WindowFlags windowFlags):
    QDialog(parent, windowFlags),
    m_ignoreAccept(false)
{
    setWindowTitle(tr("Camera settings"));

    m_settingsWidget = new QnCameraSettingsWidget(this);

    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    m_applyButton = m_buttonBox->button(QDialogButtonBox::Apply);
    m_okButton = m_buttonBox->button(QDialogButtonBox::Ok);
    
    m_openButton = new QPushButton(tr("Open in New Tab"));
    m_buttonBox->addButton(m_openButton, QDialogButtonBox::HelpRole);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_settingsWidget);
    layout->addWidget(m_buttonBox);

    connect(m_buttonBox,        SIGNAL(accepted()),                 this,   SLOT(acceptIfSafe()));
    connect(m_buttonBox,        SIGNAL(rejected()),                 this,   SLOT(reject()));
    connect(m_buttonBox,        SIGNAL(clicked(QAbstractButton *)), this,   SLOT(at_buttonBox_clicked(QAbstractButton *)));
    connect(m_settingsWidget,   SIGNAL(hasChangesChanged()),        this,   SLOT(at_settingsWidget_hasChangesChanged()));
    connect(m_settingsWidget,   SIGNAL(modeChanged()),              this,   SLOT(at_settingsWidget_modeChanged()));
    connect(m_settingsWidget,   SIGNAL(advancedSettingChanged()),   this,   SLOT(at_advancedSettingChanged()));
    connect(m_settingsWidget,   SIGNAL(fisheyeSettingChanged()),    this,   SIGNAL(fisheyeSettingChanged()));
    connect(m_settingsWidget,   SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)), this, SIGNAL(scheduleExported(const QnVirtualCameraResourceList &)));
    connect(m_openButton,       SIGNAL(clicked()),                  this,   SIGNAL(cameraOpenRequested()));

    at_settingsWidget_hasChangesChanged();
}

QnCameraSettingsDialog::~QnCameraSettingsDialog() {
    return;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraSettingsDialog::at_settingsWidget_hasChangesChanged() {
    bool hasChanges = m_settingsWidget->hasDbChanges() || m_settingsWidget->hasAnyCameraChanges();
    m_applyButton->setEnabled(hasChanges);
    m_settingsWidget->setExportScheduleButtonEnabled(!hasChanges);
}

void QnCameraSettingsDialog::at_settingsWidget_modeChanged() {
    QnCameraSettingsWidget::Mode mode = m_settingsWidget->mode();
    m_okButton->setEnabled(mode == QnCameraSettingsWidget::SingleMode || mode == QnCameraSettingsWidget::MultiMode);
    m_openButton->setVisible(mode == QnCameraSettingsWidget::SingleMode);
}

void QnCameraSettingsDialog::at_advancedSettingChanged() {
    emit advancedSettingChanged();
}

void QnCameraSettingsDialog::at_buttonBox_clicked(QAbstractButton *button) {
    emit buttonClicked(m_buttonBox->standardButton(button));
}

void QnCameraSettingsDialog::acceptIfSafe() {
    if (m_ignoreAccept)
        m_ignoreAccept = false;
    else
        accept();
}

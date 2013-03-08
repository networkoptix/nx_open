#include "export_camera_settings_dialog.h"
#include "ui_export_camera_settings_dialog.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <licensing/license.h>

#include <ui/models/resource_pool_model.h>
#include <ui/style/warning_style.h>
#include <ui/workbench/workbench_context.h>


QnExportCameraSettingsDialog::QnExportCameraSettingsDialog(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::QnExportCameraSettingsDialog()),
    m_recordingEnabled(true),
    m_motionUsed(false),
    m_dualStreamingUsed(false),
    m_licensesOk(true),
    m_motionOk(true),
    m_dtsOk(true)
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));
    ui->resourcesWidget->setModel(m_resourceModel);
    ui->resourcesWidget->setFilterVisible(true);

    setWarningStyle(ui->motionLabel);
    ui->motionLabel->setVisible(false);

    setWarningStyle(ui->dtsLabel);
    ui->dtsLabel->setVisible(false);

    at_resourceModel_dataChanged();
}

QnExportCameraSettingsDialog::~QnExportCameraSettingsDialog() {
}

QnVirtualCameraResourceList QnExportCameraSettingsDialog::getSelectedCameras() const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //servers
        QModelIndex idx = m_resourceModel->index(i, Qn::NameColumn);
        for (int j = 0; j < m_resourceModel->rowCount(idx); ++j){
            //cameras
            QModelIndex camIdx = m_resourceModel->index(j, Qn::NameColumn, idx);
            QModelIndex checkedIdx = camIdx.sibling(camIdx.row(), Qn::CheckColumn);
            bool checked = checkedIdx.data(Qt::CheckStateRole) == Qt::Checked;
            if (!checked)
                continue;
            QnResourcePtr resource = camIdx.data(Qn::ResourceRole).value<QnResourcePtr>();
            if(resource)
                 result.append(resource);
        }
    }
    QnVirtualCameraResourceList cameras = result.filtered<QnVirtualCameraResource>(); //safety check
    return cameras;
}

void QnExportCameraSettingsDialog::setRecordingEnabled(bool enabled) {
    if (m_recordingEnabled == enabled)
        return;
    m_recordingEnabled = enabled;
    updateLicensesStatus();
}

void QnExportCameraSettingsDialog::setMotionParams(bool motionUsed, bool dualStreamingUsed) {
    m_motionUsed = motionUsed;
    m_dualStreamingUsed = dualStreamingUsed;
    updateMotionStatus();
}

void QnExportCameraSettingsDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return) {
        event->ignore();
        return;
    }
    base_type::keyPressEvent(event);
}

// ------------ Handlers ---------------------

void QnExportCameraSettingsDialog::at_resourceModel_dataChanged(){
    updateLicensesStatus();
    updateMotionStatus();
    updateDtsStatus();
}

void QnExportCameraSettingsDialog::updateLicensesStatus(){
    int activeAnalog = 0;
    int activeDigital = 0;

    int countDigital = 0;
    int countAnalog = 0;
    QnVirtualCameraResourceList cameras = getSelectedCameras();
    foreach (const QnVirtualCameraResourcePtr &camera, cameras) {
        if (camera->isAnalog())
            countAnalog++;
        else
            countDigital++;

        if (!camera->isScheduleDisabled()) {
            if (camera->isAnalog())
                activeAnalog++;
            else
                activeDigital++;
        }
    }

    // how many licensed will be used if OK clicked
    int usedDigital = qnResPool->activeDigital() - activeDigital;
    int usedAnalog = qnResPool->activeAnalog() - activeAnalog;
    if (m_recordingEnabled) {
        usedDigital += countDigital;
        usedAnalog += countAnalog;
    }

    // how many licenses do we have
    int totalDigital = qnLicensePool->getLicenses().totalDigital();
    int totalAnalog = qnLicensePool->getLicenses().totalAnalog();

    QPalette palette = this->palette();
    m_licensesOk = usedDigital <= totalDigital && usedAnalog <= totalAnalog;
    if(!m_licensesOk)
        setWarningStyle(&palette);
    ui->licenseLabel->setPalette(palette);

    QString usageText = tr("%1 digital license(s) will be used out of %2.\n"\
                           "%3 analog  license(s) will be used out of %4.")
            .arg(usedDigital)
            .arg(totalDigital)
            .arg(usedAnalog)
            .arg(totalAnalog);
    ui->licenseLabel->setText(usageText);

    updateOkStatus();
}

void QnExportCameraSettingsDialog::updateMotionStatus(){
    m_motionOk = true;

    if (m_motionUsed){
        QnVirtualCameraResourceList cameras = getSelectedCameras();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras){
            bool hasMotion = /*camera->supportedMotionType() != Qn::MT_NoMotion &&*/ camera->getMotionType() != Qn::MT_NoMotion;
            if (!hasMotion) {
                m_motionOk = false;
                break;
            }
            if (m_dualStreamingUsed && !camera->hasDualStreaming()){
                m_motionOk = false;
                break;
            }
        }
    }
    ui->motionLabel->setVisible(!m_motionOk);
    updateOkStatus();
}

void QnExportCameraSettingsDialog::updateDtsStatus() {
    m_dtsOk = true;

    QnVirtualCameraResourceList cameras = getSelectedCameras();
    foreach (const QnVirtualCameraResourcePtr &camera, cameras){
        if (camera->isDtsBased()) {
            m_dtsOk = false;
            break;
        }
    }
    ui->dtsLabel->setVisible(!m_dtsOk);
    updateOkStatus();
}

void QnExportCameraSettingsDialog::updateOkStatus(){
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_licensesOk && m_motionOk && m_dtsOk);
}

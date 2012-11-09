#include "export_camera_settings_dialog.h"
#include "ui_export_camera_settings_dialog.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <licensing/license.h>

#include <ui/models/resource_pool_model.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>


QnExportCameraSettingsDialog::QnExportCameraSettingsDialog(QWidget *parent, QnWorkbenchContext *context) :
    QDialog(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnExportCameraSettingsDialog()),
    m_recordingEnabled(true),
    m_motionUsed(false),
    m_dualStreamingUsed(false),
    m_licensesOk(true),
    m_motionOk(true)
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));
    ui->resourcesWidget->setModel(m_resourceModel);

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->motionLabel->setPalette(palette);
    ui->motionLabel->setVisible(false);

    at_resourceModel_dataChanged();
}

QnExportCameraSettingsDialog::~QnExportCameraSettingsDialog() {

}

QnVirtualCameraResourceList QnExportCameraSettingsDialog::getSelectedCameras() const {
    QnResourceList result;
    for (int i = 0; i < m_resourceModel->rowCount(); ++i){
        //servers
        QModelIndex idx = m_resourceModel->index(i, 0);
        for (int j = 0; j < m_resourceModel->rowCount(idx); ++j){
            //cameras
            QModelIndex camIdx = m_resourceModel->index(j, 0, idx);//TODO: #gdm move out enum values
            QModelIndex checkedIdx = camIdx.sibling(camIdx.row(), 1); //TODO: #gdm move out enum values
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

void QnExportCameraSettingsDialog::setRecordingEnabled(bool enabled){
    if (m_recordingEnabled == enabled)
        return;
    m_recordingEnabled = enabled;
    updateLicensesStatus();
}

void QnExportCameraSettingsDialog::setMotionParams(bool motionUsed, bool dualStreamingUsed){
    m_motionUsed = motionUsed;
    m_dualStreamingUsed = dualStreamingUsed;
    updateMotionStatus();
}

void QnExportCameraSettingsDialog::at_resourceModel_dataChanged(){
    updateLicensesStatus();
    updateMotionStatus();
}

void QnExportCameraSettingsDialog::updateLicensesStatus(){
    int alreadyActive = 0;

    QnVirtualCameraResourceList cameras = getSelectedCameras();
    foreach (const QnVirtualCameraResourcePtr &camera, cameras)
        if (!camera->isScheduleDisabled())
            alreadyActive++;

    // how many licensed will be used if OK clicked
    int used = qnResPool->activeCameras() - alreadyActive;
    if (m_recordingEnabled)
        used += cameras.size();

    // how many licenses do we have
    int total = qnLicensePool->getLicenses().totalCameras();

    QPalette palette = this->palette();
    if(used > total)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->licenseLabel->setPalette(palette);

    QString usageText = tr("%n license(s) will be used out of %1.", NULL, used).arg(total);
    ui->licenseLabel->setText(usageText);

    m_licensesOk = used <= total;
    updateOkStatus();
}

void QnExportCameraSettingsDialog::updateMotionStatus(){
    m_motionOk = true;

    if (m_motionUsed){
        QnVirtualCameraResourceList cameras = getSelectedCameras();
        foreach (const QnVirtualCameraResourcePtr &camera, cameras){
            bool hasMotion = /*camera->supportedMotionType() != MT_NoMotion &&*/ camera->getMotionType() != MT_NoMotion;
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

void QnExportCameraSettingsDialog::updateOkStatus(){
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(m_licensesOk && m_motionOk);
}

#include "resource_tree_dialog.h"
#include "ui_resource_tree_dialog.h"

#include <core/resource_managment/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <licensing/license.h>

#include <ui/models/resource_pool_model.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>


QnResourceTreeDialog::QnResourceTreeDialog(QWidget *parent, QnWorkbenchContext *context) :
    QDialog(parent),
    QnWorkbenchContextAware(context ? static_cast<QObject *>(context) : parent),
    ui(new Ui::QnResourceTreeDialog()),
    m_recordingEnabled(true)
{
    ui->setupUi(this);

    m_resourceModel = new QnResourcePoolModel(this, Qn::ServersNode);
    connect(m_resourceModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(at_resourceModel_dataChanged()));
    ui->resourcesWidget->setModel(m_resourceModel);
}

QnResourceTreeDialog::~QnResourceTreeDialog() {

}

QnVirtualCameraResourceList QnResourceTreeDialog::getSelectedCameras() const {
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

void QnResourceTreeDialog::setRecordingEnabled(bool enabled){
    if (m_recordingEnabled == enabled)
        return;
    m_recordingEnabled = enabled;
    updateLicensesLabelText();
}

void QnResourceTreeDialog::at_resourceModel_dataChanged(){
    updateLicensesLabelText();
}

void QnResourceTreeDialog::updateLicensesLabelText(){
    int alreadyActive = 0;

    QnVirtualCameraResourceList cameras = getSelectedCameras();
    foreach (const QnVirtualCameraResourcePtr &camera, cameras)
        if (!camera->isScheduleDisabled())
            alreadyActive++;

    // how many licensed is used really
    int used = qnResPool->activeCameras() + cameras.size() - alreadyActive;

    // how many licenses do we have
    int total = qnLicensePool->getLicenses().totalCameras();

    QPalette palette = this->palette();
    if(used > total)
        palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->licenseLabel->setPalette(palette);

    QString usageText = tr("%n license(s) are used out of %1.", NULL, used).arg(total);

    if (m_recordingEnabled) {
        ui->licenseLabel->setText(usageText);
 /*   } else if (toBeUsed > total){
        ui->licenseLabel->setText(
            QString(QLatin1String("%1 %2")).
                arg(tr("Activate %n more license(s).", NULL, toBeUsed - total)).
                arg(usageText)
        ); */
    } else {
        ui->licenseLabel->setText(
            QString(QLatin1String("%1 %2")).
                arg(tr("%n license(s) will be used.", NULL, cameras.size() - alreadyActive)).
                arg(usageText)
        );
    }

}

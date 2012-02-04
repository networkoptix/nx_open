#include "logindialog.h"
#include "ui_camerasettingsdialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "ui/device_settings/camera_schedule_widget.h"
#include "camerasettingsdialog.h"

#include "settings.h"

CameraSettingsDialog::CameraSettingsDialog(QWidget *parent, QnVirtualCameraResourcePtr camera) :
    QDialog(parent),
    ui(new Ui::CameraSettingsDialog),
    m_cameraScheduleWidget(new CameraScheduleWidget(this)),
    m_camera(camera),
    m_connection (QnAppServerConnectionFactory::createConnection())
{
    ui->setupUi(this);
    ui->tabRecording->setLayout(new QVBoxLayout(this));
    ui->tabRecording->layout()->addWidget(m_cameraScheduleWidget);

//    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    updateView();
}

CameraSettingsDialog::~CameraSettingsDialog()
{
}

void CameraSettingsDialog::requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle)
{
    if (status == 0) {
        QDialog::accept();
    } else {
        QMessageBox::warning(this, "Can't save camera", "Can't save camera. Please try again later.");
        ui->buttonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    }
}

void CameraSettingsDialog::accept()
{
    ui->buttonBox->setEnabled(false);

    saveToModel();
    save();
}


void CameraSettingsDialog::reject()
{
    QDialog::reject();
}

void CameraSettingsDialog::saveToModel()
{
    m_camera->setName(ui->nameEdit->text());
    m_camera->setHostAddress(QHostAddress(ui->ipAddressEdit->text()));
    m_camera->setAuth(ui->loginEdit->text(), ui->passwordEdit->text());
    m_camera->setScheduleTasks(m_cameraScheduleWidget->scheduleTasks());
}

void CameraSettingsDialog::updateView()
{
    ui->nameEdit->setText(m_camera->getName());
    ui->macAddressValueLabel->setText(m_camera->getMAC().toString());
    ui->ipAddressEdit->setText(m_camera->getHostAddress().toString());
    ui->loginEdit->setText(m_camera->getAuth().user());
    ui->passwordEdit->setText(m_camera->getAuth().password());

    m_cameraScheduleWidget->setScheduleTasks(m_camera->getScheduleTasks());
}

void CameraSettingsDialog::buttonClicked(QAbstractButton *button)
{
    if (ui->buttonBox->standardButton(button) == QDialogButtonBox::Apply)
    {
        saveToModel();
        save();
    }
}

void CameraSettingsDialog::save()
{
    m_connection->saveAsync(m_camera, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}


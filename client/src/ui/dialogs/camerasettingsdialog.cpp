#include "logindialog.h"
#include "ui_camerasettingsdialog.h"

#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include "core/resource/resource.h"
#include "ui/preferences/preferencesdialog.h"
#include "ui/style/skin.h"
#include "camerasettingsdialog.h"

#include "settings.h"

CameraSettingsDialog::CameraSettingsDialog(QWidget *parent, QnVirtualCameraResourcePtr camera) :
    QDialog(parent),
    ui(new Ui::CameraSettingsDialog),
    m_camera(camera)
{
    ui->setupUi(this);

    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
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
    }
}

void CameraSettingsDialog::accept()
{
    ui->buttonBox->setEnabled(false);

    connection = QnAppServerConnectionFactory::createConnection();
    connection->saveAsync(m_camera, this, SLOT(requestFinished(int,QByteArray,QnResourceList,int)));
}


void CameraSettingsDialog::reject()
{
    QDialog::reject();
}


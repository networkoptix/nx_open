#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <functional>

#include <QtCore/QUuid>
#include <QtGui/QDataWidgetMapper>
#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QSpinBox>

#include <utils/common/counter.h>

#include <core/resource/storage_resource.h>
#include <core/resource/video_server_resource.h>

QnCameraAdditionDialog::QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(server)
{
    ui->setupUi(this);

    setButtonBox(ui->buttonBox);

    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLabel, SLOT(setVisible(bool)));
    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLineEdit, SLOT(setVisible(bool)));
    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), this, SLOT(at_singleRadioButton_toggled(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLineEdit, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLineEdit, SLOT(setVisible(bool)));

    ui->startIPLabel->setVisible(false);
    ui->startIPLineEdit->setVisible(false);
    ui->endIPLabel->setVisible(false);
    ui->endIPLineEdit->setVisible(false);
    ui->scanProgressBar->setVisible(false);
    ui->camerasGroupBox->setVisible(false);
    ui->stopScanButton->setVisible(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));
}

QnCameraAdditionDialog::~QnCameraAdditionDialog(){}

void QnCameraAdditionDialog::fillTable(const QnCamerasFoundInfoList &cameras) {

    ui->camerasTable->setRowCount(0);

    foreach(QnCamerasFoundInfo info, cameras){
        int row = ui->camerasTable->rowCount();
        ui->camerasTable->insertRow(row);

        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setFlags(checkItem->flags() | Qt::ItemIsUserCheckable);
        checkItem->setCheckState(Qt::Unchecked);

        QTableWidgetItem *addressItem = new QTableWidgetItem(info.address);
        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);

        ui->camerasTable->setItem(row, 0, checkItem);
        ui->camerasTable->setItem(row, 1, addressItem);
        ui->camerasTable->setItem(row, 2, nameItem);
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnCameraAdditionDialog::at_scanButton_clicked(){

    QString startAddr;
    QString endAddr;
    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());
    if (ui->rangeRadioButton->isChecked()){
        startAddr = ui->startIPLineEdit->text();
        endAddr = ui->endIPLineEdit->text();
    }else{
        startAddr = ui->iPAddressLineEdit->text();
        endAddr = startAddr;
    }
    qDebug() << "start" <<startAddr << "end" << endAddr;

    QHostAddress addr1(ui->iPAddressLineEdit->text());
    if (addr1.isNull())
        return;
    //TODO: #gdm show message dlg or better write red label


    ui->buttonBox->setEnabled(false);
    ui->camerasGroupBox->setVisible(false);
    ui->scanProgressBar->setVisible(true);
    ui->stopScanButton->setVisible(true);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::CheckCamerasFoundReplyProcessor> processor(new detail::CheckCamerasFoundReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    int handle = serverConnection->asyncGetCameraAddition(processor.data(), SLOT(processReply(const QnCamerasFoundInfoList &)),
                                                          startAddr, endAddr, username, password);
    Q_UNUSED(handle)

    eventLoop->exec();

    ui->stopScanButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);
    ui->camerasGroupBox->setVisible(true);
    fillTable(processor->camerasFound());
}

void QnCameraAdditionDialog::at_singleRadioButton_toggled(bool toggled){
    if (toggled)
        ui->iPAddressLineEdit->setText(ui->startIPLineEdit->text());
    else
        ui->startIPLineEdit->setText(ui->iPAddressLineEdit->text());
}

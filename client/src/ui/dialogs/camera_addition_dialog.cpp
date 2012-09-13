#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <ui/style/globals.h>

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
    ui->validateLabel->setVisible(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));
    connect(ui->iPAddressLineEdit, SIGNAL(editingFinished()), this, SLOT(at_scanButton_clicked()));

    ui->loginLineEdit->installEventFilter(this);
    ui->passwordLineEdit->installEventFilter(this);
    ui->startIPLineEdit->installEventFilter(this);
    ui->endIPLineEdit->installEventFilter(this);
    ui->iPAddressLineEdit->installEventFilter(this);

    ui->iPAddressLineEdit->setFocus();

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->validateLabel->setPalette(palette);
}

QnCameraAdditionDialog::~QnCameraAdditionDialog(){}

bool QnCameraAdditionDialog::eventFilter(QObject *object, QEvent *event){
    if (event->type() == QEvent::KeyPress && ((QKeyEvent*)event)->key() == Qt::Key_Return) {
        if (object == ui->loginLineEdit ||
            object == ui->passwordLineEdit ||
            object == ui->startIPLineEdit ||
            object == ui->endIPLineEdit ||
            object == ui->iPAddressLineEdit)
        ui->scanButton->setFocus();
        event->ignore();
        return false;
    }
    return QnButtonBoxDialog::eventFilter(object, event);
}

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


    // TODO: #gdm move validation to QnIpLineEdit
    if (QHostAddress(startAddr).isNull()){
        ui->validateLabel->setText(tr("Ip address \"%1\" is invalid").arg(startAddr));
        ui->validateLabel->setVisible(true);
        return;
    }

    if (QHostAddress(endAddr).isNull()){
        ui->validateLabel->setText(tr("Ip address \"%1\" is invalid").arg(endAddr));
        ui->validateLabel->setVisible(true);
        return;
    }

    ui->validateLabel->setVisible(false);

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

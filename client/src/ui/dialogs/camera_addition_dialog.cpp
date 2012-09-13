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
    ui->stopScanButton->setVisible(false);
    ui->validateLabelSearch->setVisible(false);

    ui->addProgressBar->setVisible(false);
    ui->stopAddButton->setVisible(false);
    ui->addPage->setVisible(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));
    connect(ui->iPAddressLineEdit, SIGNAL(editingFinished()), this, SLOT(at_scanButton_clicked()));

    ui->loginLineEdit->installEventFilter(this);
    ui->passwordLineEdit->installEventFilter(this);
    ui->startIPLineEdit->installEventFilter(this);
    ui->endIPLineEdit->installEventFilter(this);
    ui->iPAddressLineEdit->installEventFilter(this);
    ui->iPAddressLineEdit->setFocus();

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->validateLabelSearch->setPalette(palette);
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
        checkItem->setData(Qt::UserRole, info.url);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);

        ui->camerasTable->setItem(row, 0, checkItem);
        ui->camerasTable->setItem(row, 1, nameItem);
    }
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //


void QnCameraAdditionDialog::at_singleRadioButton_toggled(bool toggled){
    if (toggled)
        ui->iPAddressLineEdit->setText(ui->startIPLineEdit->text());
    else
        ui->startIPLineEdit->setText(ui->iPAddressLineEdit->text());
}

void QnCameraAdditionDialog::at_scanButton_clicked(){
    QString startAddr;
    QString endAddr;
    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());
    int port = ui->portSpinBox->value();

    if (ui->rangeRadioButton->isChecked()){
        startAddr = ui->startIPLineEdit->text();
        endAddr = ui->endIPLineEdit->text();
    }else{
        startAddr = ui->iPAddressLineEdit->text();
        endAddr = startAddr;
    }


    // TODO: #gdm move validation to QnIpLineEdit
    if (QHostAddress(startAddr).isNull()){
        ui->validateLabelSearch->setText(tr("Ip address \"%1\" is invalid").arg(startAddr));
        ui->validateLabelSearch->setVisible(true);
        return;
    }

    if (QHostAddress(endAddr).isNull()){
        ui->validateLabelSearch->setText(tr("Ip address \"%1\" is invalid").arg(endAddr));
        ui->validateLabelSearch->setVisible(true);
        return;
    }

    ui->validateLabelSearch->setVisible(false);

    ui->buttonBox->setEnabled(false);
//    ui->camerasGroupBox->setVisible(false);
    ui->scanProgressBar->setVisible(true);
    ui->stopScanButton->setVisible(true);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraSearch(processor.data(), SLOT(processSearchReply(const QnCamerasFoundInfoList &)),
                                                 startAddr, endAddr, username, password, port);

    eventLoop->exec();

    ui->stopScanButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);
//    ui->camerasGroupBox->setVisible(true);
    // if (processor->camerasFound().count() > 0){
    //
    ui->addPage->setVisible(true);
    ui->stagesToolBox->setCurrentIndex(ui->stagesToolBox->indexOf(ui->addPage));
    fillTable(processor->camerasFound());
}

void QnCameraAdditionDialog::at_addButton_clicked(){

    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());

    QStringList urls;
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, 0)->checkState() != Qt::Checked)
            continue;

        //QnCamerasFoundInfo info = ui->camerasTable->item(row, 0)->data(Qt::UserRole);
        //qDebug() << "output info item checked" << info.name << info.manufacture << info.url;
        urls.append(QString(ui->camerasTable->item(row, 0)->data(Qt::UserRole).toString()));
    }

    ui->buttonBox->setEnabled(false);
    ui->addProgressBar->setVisible(true);
    ui->stopAddButton->setVisible(true);
    ui->searchPage->setEnabled(false);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopAddButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopAddButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraAdd(processor.data(), SLOT(processAddReply(int)),
                                              urls, QString(), username, password);

    eventLoop->exec();

    ui->stopScanButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);
    ui->searchPage->setEnabled(true);
//    ui->camerasGroupBox->setVisible(true);
}


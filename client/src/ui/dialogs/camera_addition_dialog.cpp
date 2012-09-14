#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>

#include <ui/style/globals.h>

#include <core/resource/video_server_resource.h>

QnCameraAdditionDialog::QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(server),
    m_inIpRangeEdit(false)
{
    ui->setupUi(this);

    ui->camerasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->camerasTable->horizontalHeader()->setResizeMode(0, QHeaderView::ResizeToContents);
    ui->camerasTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->camerasTable->horizontalHeader()->setResizeMode(2, QHeaderView::Stretch);
    ui->camerasTable->horizontalHeader()->setVisible(true); /* Qt designer does not save this flag (probably a bug in Qt designer). */

    setButtonBox(ui->buttonBox);

    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLabel, SLOT(setVisible(bool)));
    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), ui->iPAddressLineEdit, SLOT(setVisible(bool)));
    connect(ui->singleRadioButton,  SIGNAL(toggled(bool)), this, SLOT(at_singleRadioButton_toggled(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->startIPLineEdit, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLabel, SLOT(setVisible(bool)));
    connect(ui->rangeRadioButton,   SIGNAL(toggled(bool)), ui->endIPLineEdit, SLOT(setVisible(bool)));
    connect(ui->startIPLineEdit,    SIGNAL(textChanged(QString)), this, SLOT(at_startIPLineEdit_textChanged(QString)));
    connect(ui->endIPLineEdit,    SIGNAL(textChanged(QString)), this, SLOT(at_endIPLineEdit_textChanged(QString)));

    ui->startIPLabel->setVisible(false);
    ui->startIPLineEdit->setVisible(false);
    ui->endIPLabel->setVisible(false);
    ui->endIPLineEdit->setVisible(false);
    ui->scanProgressBar->setVisible(false);
    ui->stopScanButton->setVisible(false);
    ui->validateLabelSearch->setVisible(false);

    ui->addProgressBar->setVisible(false);
    ui->stopAddButton->setVisible(false);

    ui->stagesToolBox->setItemEnabled(ui->stagesToolBox->indexOf(ui->addPage), false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));

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
        {
            ui->scanButton->setFocus();
            qDebug() << "scan button focused";
            event->ignore();
            return false;
        }
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

        QTableWidgetItem *manufItem = new QTableWidgetItem(info.manufacturer);
        manufItem->setToolTip(info.url);
        manufItem->setFlags(manufItem->flags() &~ Qt::ItemIsEditable);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
        nameItem->setToolTip(info.url);
        nameItem->setFlags(nameItem->flags() &~ Qt::ItemIsEditable);

        ui->camerasTable->setItem(row, 0, checkItem);
        ui->camerasTable->setItem(row, 1, manufItem);
        ui->camerasTable->setItem(row, 2, nameItem);
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

void QnCameraAdditionDialog::at_startIPLineEdit_textChanged(QString value){
    if (m_inIpRangeEdit)
        return;

    m_inIpRangeEdit = true;

    QString fixed(value);
    ui->startIPLineEdit->validator()->fixup(fixed);

    QHostAddress startAddr(fixed);
    QHostAddress endAddr(ui->endIPLineEdit->text());

    if (startAddr.toIPv4Address() > endAddr.toIPv4Address() ||
        endAddr.toIPv4Address() - startAddr.toIPv4Address() > 255){
        endAddr = QHostAddress::parseSubnet(startAddr.toString() + QLatin1String("/24")).first;
        endAddr = QHostAddress(endAddr.toIPv4Address() + 255);

        ui->endIPLineEdit->setText(endAddr.toString());
    }
    m_inIpRangeEdit = false;
}

void QnCameraAdditionDialog::at_endIPLineEdit_textChanged(QString value){
    if (m_inIpRangeEdit)
        return;

    m_inIpRangeEdit = true;
    QHostAddress startAddr(ui->startIPLineEdit->text());

    QString fixed(value);
    ui->endIPLineEdit->validator()->fixup(fixed);
    QHostAddress endAddr(fixed);

    if (endAddr.toIPv4Address() > startAddr.toIPv4Address() &&
        endAddr.toIPv4Address() - startAddr.toIPv4Address() > 255){
        startAddr = QHostAddress::parseSubnet(endAddr.toString() + QLatin1String("/24")).first;
        ui->startIPLineEdit->setText(startAddr.toString());
    }
    m_inIpRangeEdit = false;
}

void QnCameraAdditionDialog::at_scanButton_clicked(){
    QString startAddrStr;
    QString endAddrStr;
    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());
    int port = ui->portSpinBox->value();

    if (ui->rangeRadioButton->isChecked()){
        startAddrStr = ui->startIPLineEdit->text();
        endAddrStr = ui->endIPLineEdit->text();
    }else{
        startAddrStr = ui->iPAddressLineEdit->text();
        endAddrStr = startAddrStr;
    }

    QHostAddress startAddr(startAddrStr);
    QHostAddress endAddr(endAddrStr);


    //TODO: #gdm: not used anymore
    if (startAddr.toIPv4Address() > endAddr.toIPv4Address()){
        ui->validateLabelSearch->setText(tr("First address in range is greater than last one"));
        ui->validateLabelSearch->setVisible(true);
        return;
    }

    if (!endAddr.isInSubnet(startAddr, 8)){
        ui->validateLabelSearch->setText(tr("Ip address range is too big"));
        ui->validateLabelSearch->setVisible(true);
        return;
    }


    ui->validateLabelSearch->setVisible(false);
    ui->buttonBox->setEnabled(false);
    ui->scanProgressBar->setVisible(true);
    ui->stopScanButton->setVisible(true);
    ui->scanButton->setVisible(false);
    ui->stagesToolBox->setItemEnabled(ui->stagesToolBox->indexOf(ui->addPage), false);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraSearch(processor.data(), SLOT(processSearchReply(const QnCamerasFoundInfoList &)),
                                                 startAddrStr, endAddrStr, username, password, port);

    eventLoop->exec();

    ui->scanButton->setVisible(true);
    ui->stopScanButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);

    if (!processor->isCancelled()){
        if (processor->camerasFound().count() > 0){
            ui->stagesToolBox->setItemEnabled(ui->stagesToolBox->indexOf(ui->addPage), true);
            ui->stagesToolBox->setCurrentIndex(ui->stagesToolBox->indexOf(ui->addPage));
            fillTable(processor->camerasFound());
        } else {
            QMessageBox::information(this, tr("Finished"), tr("No cameras found"), QMessageBox::Ok);
        }
    }
}

void QnCameraAdditionDialog::at_addButton_clicked(){

    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());

    QStringList urls;
    QStringList manufacturers;
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, 0)->checkState() != Qt::Checked)
            continue;

        //QnCamerasFoundInfo info = ui->camerasTable->item(row, 0)->data(Qt::UserRole);
        //qDebug() << "output info item checked" << info.name << info.manufacture << info.url;
        urls.append(ui->camerasTable->item(row, 0)->data(Qt::UserRole).toString());
        manufacturers.append(ui->camerasTable->item(row, 1)->text());
    }
    if (urls.empty()){
        QMessageBox::information(this, tr("No cameras selected"), tr("Please select at least one camera"), QMessageBox::Ok);
        return;
    }


    ui->buttonBox->setEnabled(false);
    ui->addProgressBar->setVisible(true);
    ui->stopAddButton->setVisible(true);
    ui->addButton->setVisible(false);
    ui->stagesToolBox->setItemEnabled(ui->stagesToolBox->indexOf(ui->searchPage), false);


    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopAddButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopAddButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraAdd(processor.data(), SLOT(processAddReply(int)),
                                              urls, manufacturers, username, password);

    eventLoop->exec();

    ui->stopAddButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->addProgressBar->setVisible(false);
    ui->stagesToolBox->setItemEnabled(ui->stagesToolBox->indexOf(ui->searchPage), true);
    ui->addButton->setVisible(true);

    if (!processor->isCancelled()){
        if (processor->addSuccess())
            QMessageBox::information(this, tr("Success"), tr("Camera(s) added successfully"), QMessageBox::Ok);
        else
            QMessageBox::critical(this, tr("Error"), tr("Error while adding camera(s)"), QMessageBox::Ok);
    }
}


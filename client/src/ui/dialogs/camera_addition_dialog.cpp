#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QDesktopServices>

#include <ui/style/globals.h>

#include <core/resource/video_server_resource.h>

QnCameraAdditionDialog::QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(server),
    m_inIpRangeEdit(false)
{
    ui->setupUi(this);

    ui->camerasTable->horizontalHeader()->setResizeMode(1, QHeaderView::ResizeToContents);
    ui->camerasTable->horizontalHeader()->setResizeMode(2, QHeaderView::ResizeToContents);

    setButtonBox(ui->buttonBox);

    connect(ui->startIPLineEdit,    SIGNAL(textChanged(QString)), this, SLOT(at_startIPLineEdit_textChanged(QString)));
    connect(ui->startIPLineEdit,    SIGNAL(editingFinished()),    this, SLOT(at_startIPLineEdit_editingFinished()));
    connect(ui->endIPLineEdit,      SIGNAL(textChanged(QString)), this, SLOT(at_endIPLineEdit_textChanged(QString)));
    connect(ui->camerasTable,       SIGNAL(cellChanged(int,int)), this, SLOT(at_camerasTable_cellChanged(int, int)));
    connect(ui->camerasTable,       SIGNAL(cellClicked(int,int)), this, SLOT(at_camerasTable_cellClicked(int, int)));

    ui->scanProgressBar->setVisible(false);
    ui->stopScanButton->setVisible(false);
    ui->validateLabelSearch->setVisible(false);

    ui->addProgressBar->setVisible(false);
    ui->stopAddButton->setVisible(false);
    ui->addButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));

    ui->loginLineEdit->installEventFilter(this);
    ui->passwordLineEdit->installEventFilter(this);
    ui->startIPLineEdit->installEventFilter(this);
    ui->endIPLineEdit->installEventFilter(this);
    ui->portSpinBox->installEventFilter(this);

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
            object == ui->portSpinBox)
        {
            ui->scanButton->setFocus();
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

        QTableWidgetItem *manufItem = new QTableWidgetItem(info.manufacturer);
        manufItem->setFlags(manufItem->flags() &~ Qt::ItemIsEditable);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
        nameItem->setFlags(nameItem->flags() &~ Qt::ItemIsEditable);

        QFont font = ui->camerasTable->font();
        font.setUnderline(true);

        QTableWidgetItem *urlItem = new QTableWidgetItem(info.url);
        urlItem->setFlags(urlItem->flags() &~ Qt::ItemIsEditable);
        urlItem->setData(Qt::FontRole, font);

        ui->camerasTable->setItem(row, 0, checkItem);
        ui->camerasTable->setItem(row, 1, manufItem);
        ui->camerasTable->setItem(row, 2, nameItem);
        ui->camerasTable->setItem(row, 3, urlItem);
    }
}

void QnCameraAdditionDialog::removeAddedCameras(){
    int row = ui->camerasTable->rowCount() - 1;
    while (row > 0){
        if (ui->camerasTable->item(row, 0)->checkState() != Qt::Checked)
            continue;
        ui->camerasTable->removeRow(row);
        row--;
    }
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnCameraAdditionDialog::at_startIPLineEdit_textChanged(QString value){
    if (m_inIpRangeEdit)
        return;

    m_inIpRangeEdit = true;

    QString fixed(value);
    ui->startIPLineEdit->validator()->fixup(fixed);

    QHostAddress startAddr(fixed);
    ui->endIPLineEdit->setText(startAddr.toString());
    m_inIpRangeEdit = false;
}

void QnCameraAdditionDialog::at_startIPLineEdit_editingFinished(){
    QHostAddress startAddr(ui->endIPLineEdit->text());
    if (startAddr.toIPv4Address() % 256 == 0){
        qDebug() << "is subnet";
        QHostAddress endAddr(startAddr.toIPv4Address() + 255);
        ui->endIPLineEdit->setText(endAddr.toString());
    }
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

void QnCameraAdditionDialog::at_camerasTable_cellChanged( int row, int column){
    Q_UNUSED(row)

    if (column > 0)
        return;

    int rowCount = ui->camerasTable->rowCount();
    bool enabled = rowCount > 0;
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, 0)->checkState() == Qt::Checked)
            continue;
        enabled = false;
        break;
    }
    ui->addButton->setEnabled(enabled);
}

void QnCameraAdditionDialog::at_camerasTable_cellClicked(int row, int column){
    if (column != 3)
        return;

    QUrl url(ui->camerasTable->item(row, column)->text());
    if (url.isEmpty())
        return;

    url.setPath(QString());
    qDebug() << "opening url" << url.toString();
    QDesktopServices::openUrl(url);
}

void QnCameraAdditionDialog::at_scanButton_clicked(){
    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());
    int port = ui->portSpinBox->value();

    QString startAddrStr = ui->startIPLineEdit->text();
    QString endAddrStr = ui->endIPLineEdit->text();

    QHostAddress startAddr(startAddrStr);
    QHostAddress endAddr(endAddrStr);

    if (startAddr.toIPv4Address() > endAddr.toIPv4Address()){
        ui->validateLabelSearch->setText(tr("First address in range is greater than last one"));
        ui->validateLabelSearch->setVisible(true);
        return;
    }

    if (!endAddr.isInSubnet(startAddr, 8)){
        ui->validateLabelSearch->setText(tr("Ip address range is too big, maximum of 255 addresses is allowed"));
        ui->validateLabelSearch->setVisible(true);
        return;
    }


    ui->validateLabelSearch->setVisible(false);
    ui->buttonBox->setEnabled(false);
    ui->scanProgressBar->setVisible(true);
    ui->stopScanButton->setVisible(true);
    ui->scanButton->setEnabled(false);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnVideoServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraSearch(processor.data(), SLOT(processSearchReply(const QnCamerasFoundInfoList &)),
                                                 startAddrStr, endAddrStr, username, password, port);

    eventLoop->exec();

    ui->scanButton->setEnabled(true);
    ui->stopScanButton->setVisible(false);
    ui->buttonBox->setEnabled(true);
    ui->scanProgressBar->setVisible(false);

    if (!processor->isCancelled()){
        if (processor->camerasFound().count() > 0){
            fillTable(processor->camerasFound());
            ui->camerasTable->setEnabled(true);
            ui->addButton->setFocus();
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
        urls.append(ui->camerasTable->item(row, 3)->text());
        manufacturers.append(ui->camerasTable->item(row, 1)->text());
    }
    if (urls.empty()){
        QMessageBox::information(this, tr("No cameras selected"), tr("Please select at least one camera"), QMessageBox::Ok);
        return;
    }

    ui->buttonBox->setEnabled(false);
    ui->addProgressBar->setVisible(true);
    ui->stopAddButton->setVisible(true);
    ui->addButton->setEnabled(false);
    ui->scanButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

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
    ui->addButton->setEnabled(true);
    ui->scanButton->setEnabled(true);
    ui->camerasTable->setEnabled(true);

    if (!processor->isCancelled()){
        if (processor->addSuccess()){
            removeAddedCameras();
            QMessageBox::information(this, tr("Success"), tr("Camera(s) added successfully"), QMessageBox::Ok);
        }
        else
            QMessageBox::critical(this, tr("Error"), tr("Error while adding camera(s)"), QMessageBox::Ok);
    }
}


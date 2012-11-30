#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QDesktopServices>

#include <core/resource/media_server_resource.h>

#include <ui/style/globals.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#define PORT_AUTO 0

QnCameraAdditionDialog::QnCameraAdditionDialog(const QnMediaServerResourcePtr &server, QWidget *parent):
    QDialog(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(server),
    m_inIpRangeEdit(false),
    m_subnetMode(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::ManualCameraAddition_Help);

    ui->camerasTable->horizontalHeader()->setVisible(true);
    ui->camerasTable->horizontalHeader()->setResizeMode(QHeaderView::Interactive);
    ui->camerasTable->resizeColumnsToContents();

    connect(ui->startIPLineEdit,    SIGNAL(textChanged(QString)), this, SLOT(at_startIPLineEdit_textChanged(QString)));
    connect(ui->startIPLineEdit,    SIGNAL(editingFinished()),    this, SLOT(at_startIPLineEdit_editingFinished()));
    connect(ui->endIPLineEdit,      SIGNAL(textChanged(QString)), this, SLOT(at_endIPLineEdit_textChanged(QString)));
    connect(ui->camerasTable,       SIGNAL(cellChanged(int,int)), this, SLOT(at_camerasTable_cellChanged(int, int)));
    connect(ui->camerasTable,       SIGNAL(cellClicked(int,int)), this, SLOT(at_camerasTable_cellClicked(int, int)));
    connect(ui->subnetCheckbox,     SIGNAL(toggled(bool)),        this, SLOT(at_subnetCheckbox_toggled(bool)));
    connect(ui->closeButton,        SIGNAL(clicked()),            this, SLOT(accept()));

    connect(ui->portAutoCheckBox,   SIGNAL(toggled(bool)),        ui->portSpinBox, SLOT(setDisabled(bool)));

    ui->scanProgressBar->setVisible(false);
    ui->stopScanButton->setVisible(false);
    ui->validateLabelSearch->setVisible(false);

    ui->cameraIpLineEdit->setMinimumSize(ui->startIPLineEdit->minimumSizeHint());

    ui->addButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));

    QPalette palette = this->palette();
    palette.setColor(QPalette::WindowText, qnGlobals->errorTextColor());
    ui->validateLabelSearch->setPalette(palette);

    updateSubnetMode();
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
    while (row >= 0){
        if (ui->camerasTable->item(row, 0)->checkState() == Qt::Checked)
	        ui->camerasTable->removeRow(row);
		row--;
    }
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);
}

void QnCameraAdditionDialog::updateSubnetMode(){
    ui->startIPLabel->setVisible(m_subnetMode);
    ui->startIPLineEdit->setVisible(m_subnetMode);
    ui->endIPLabel->setVisible(m_subnetMode);
    ui->endIPLineEdit->setVisible(m_subnetMode);

    ui->cameraIpLabel->setVisible(!m_subnetMode);
    ui->cameraIpLineEdit->setVisible(!m_subnetMode);

    if (m_subnetMode){
        QHostAddress startAddr(ui->cameraIpLineEdit->text());
        if (startAddr.toIPv4Address()) {
            ui->startIPLineEdit->setText(startAddr.toString());

            quint32 addr = startAddr.toIPv4Address();
            addr = addr >> 8;
            addr = (addr << 8) + 255;
            QString endAddrStr = QHostAddress(addr).toString();
            ui->endIPLineEdit->setText(endAddrStr);
            ui->endIPLineEdit->setFocus();
            ui->endIPLineEdit->setSelection(endAddrStr.size() - 3, 3);
        } else {
            ui->startIPLineEdit->setFocus();
        }
    } else
        ui->cameraIpLineEdit->setFocus();
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
    if (ui->subnetCheckbox->isChecked() && (startAddr.toIPv4Address() % 256 == 0)){
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

    quint32 startSubnet = startAddr.toIPv4Address() >> 8;
    quint32 endSubnet = endAddr.toIPv4Address() >> 8;

    if (startSubnet != endSubnet){
        startAddr = QHostAddress(startAddr.toIPv4Address() + ((endSubnet - startSubnet) << 8) );
        ui->startIPLineEdit->setText(startAddr.toString());
    }
    m_inIpRangeEdit = false;
}

void QnCameraAdditionDialog::at_camerasTable_cellChanged( int row, int column){
    Q_UNUSED(row)

    if (column > 0)
        return;

    int rowCount = ui->camerasTable->rowCount();
    bool enabled = false;
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, 0)->checkState() != Qt::Checked)
            continue;
		enabled = true;
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
    int port = ui->portAutoCheckBox->isChecked()
            ? PORT_AUTO
            : ui->portSpinBox->value();

    QString startAddrStr;
    QString endAddrStr;
    if (m_subnetMode) {
        startAddrStr = ui->startIPLineEdit->text();
        endAddrStr = ui->endIPLineEdit->text();

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
    } else {
        QUrl url = QUrl::fromUserInput(ui->cameraIpLineEdit->text());
        if (url == QUrl()){
            ui->validateLabelSearch->setText(tr("Camera address filed must contain valid url or ip address"));
            ui->validateLabelSearch->setVisible(true);
            return;
        }
        startAddrStr = url.host();
        endAddrStr = QString();
    }

    ui->scanButton->setEnabled(false);

    ui->startIPLineEdit->setEnabled(false);
    ui->cameraIpLineEdit->setEnabled(false);
    ui->endIPLineEdit->setEnabled(false);
    ui->portAutoCheckBox->setEnabled(false);
    ui->portSpinBox->setEnabled(false);
    ui->subnetCheckbox->setEnabled(false);
    ui->loginLineEdit->setEnabled(false);
    ui->passwordLineEdit->setEnabled(false);


    ui->validateLabelSearch->setVisible(false);
    ui->scanProgressBar->setVisible(true);
    ui->stopScanButton->setVisible(true);
    ui->stopScanButton->setFocus();

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));
    connect(ui->closeButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->closeButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));


    QnMediaServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraSearch(startAddrStr, endAddrStr, username, password, port,
                                                 processor.data(),
                                                 SLOT(processSearchReply(const QnCamerasFoundInfoList &)),
                                                 SLOT(processSearchError(int, const QString &)));

    eventLoop->exec();

    ui->scanButton->setEnabled(true);
    ui->startIPLineEdit->setEnabled(true);
    ui->cameraIpLineEdit->setEnabled(true);
    ui->endIPLineEdit->setEnabled(true);
    ui->portAutoCheckBox->setEnabled(true);
    ui->portSpinBox->setEnabled(!ui->portAutoCheckBox->isChecked());
    ui->subnetCheckbox->setEnabled(true);
    ui->loginLineEdit->setEnabled(true);
    ui->passwordLineEdit->setEnabled(true);

    ui->stopScanButton->setVisible(false);
    ui->scanProgressBar->setVisible(false);

    if (!processor->isCancelled()) {
        if (!processor->isSuccess()) {
            QString processor_error = processor->getLastError();
            QString error = tr("Server returned an error:\n%1");
            if (processor_error.length() == 0){
                error = error.arg(tr("This server version supports only searching by ip address."));
            } else {
                error = error.arg(processor_error);
            }
            QMessageBox::critical(this, tr("Error"), error, QMessageBox::Ok);
        } else if (processor->camerasFound().count() > 0) {
            fillTable(processor->camerasFound());
            ui->camerasTable->setEnabled(true);
            ui->addButton->setFocus();
            ui->camerasTable->resizeColumnsToContents();
        } else {
            QMessageBox::information(this, tr("Finished"), tr("No cameras found"), QMessageBox::Ok);
            if (m_subnetMode)
                ui->startIPLineEdit->setFocus();
            else
                ui->cameraIpLineEdit->setFocus();
        }
    } else
        if (m_subnetMode)
            ui->startIPLineEdit->setFocus();
        else
            ui->cameraIpLineEdit->setFocus();
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

    ui->addButton->setEnabled(false);
    ui->scanButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

    QScopedPointer<QEventLoop> eventLoop(new QEventLoop());

    QScopedPointer<detail::ManualCameraReplyProcessor> processor(new detail::ManualCameraReplyProcessor());
    connect(processor.data(), SIGNAL(replyReceived()),  eventLoop.data(), SLOT(quit()));
    connect(ui->closeButton, SIGNAL(clicked()), eventLoop.data(), SLOT(quit()));
    connect(ui->closeButton, SIGNAL(clicked()), processor.data(), SLOT(cancel()));

    QnMediaServerConnectionPtr serverConnection = m_server->apiConnection();
    serverConnection->asyncGetManualCameraAdd(urls, manufacturers, username, password,
                                              processor.data(), SLOT(processAddReply(int)));

    eventLoop->exec();

    ui->addButton->setEnabled(true);
    ui->scanButton->setEnabled(true);
    ui->camerasTable->setEnabled(true);

    if (!processor->isCancelled()) {
        if (processor->isSuccess()) {
            removeAddedCameras();
            QMessageBox::information(this, tr("Success"), tr("Camera(s) added successfully"), QMessageBox::Ok);
        }
        else
            QMessageBox::critical(this, tr("Error"), tr("Error while adding camera(s)"), QMessageBox::Ok);
    }
}

void QnCameraAdditionDialog::at_subnetCheckbox_toggled(bool toggled){
    if (m_subnetMode == toggled)
        return;

    m_subnetMode = toggled;
    updateSubnetMode();
}

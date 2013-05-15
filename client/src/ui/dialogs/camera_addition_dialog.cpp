#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QMessageBox>
#include <QtGui/QStandardItemModel>
#include <QtGui/QDesktopServices>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/style/warning_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#define PORT_AUTO 0

namespace {
    enum Column {
        CheckBoxColumn,
        ManufColumn,
        NameColumn,
        UrlColumn
    };
}

QnCheckBoxedHeaderView::QnCheckBoxedHeaderView(QWidget *parent):
    base_type(Qt::Horizontal, parent),
    m_checkState(Qt::Unchecked)
{
    connect(this, SIGNAL(sectionClicked(int)), this, SLOT(at_sectionClicked(int)));
}

Qt::CheckState QnCheckBoxedHeaderView::checkState() const {
    return m_checkState;
}

void QnCheckBoxedHeaderView::setCheckState(Qt::CheckState state) {
    if (state == m_checkState)
        return;
    m_checkState = state;
    emit checkStateChanged(state);
}

void QnCheckBoxedHeaderView::paintEvent(QPaintEvent *e) {
    base_type::paintEvent(e);
}

void QnCheckBoxedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    base_type::paintSection(painter, rect, logicalIndex);

    if (logicalIndex == CheckBoxColumn) {
        if (!rect.isValid())
            return;
        QStyleOptionButton opt;
        opt.initFrom(this);

        QStyle::State state = QStyle::State_Raised;
        if (isEnabled())
            state |= QStyle::State_Enabled;
        if (window()->isActiveWindow())
            state |= QStyle::State_Active;

        switch(m_checkState) {
        case Qt::Checked:
            state |= QStyle::State_On;
            break;
        case Qt::Unchecked:
            state |= QStyle::State_Off;
            break;
        default:
            state |= QStyle::State_NoChange;
            break;
        }

        opt.rect = rect.adjusted(4, 0, 0, 0);
        opt.state |= state;
        opt.text = QString();
        style()->drawControl(QStyle::CE_CheckBox, &opt, painter, this);
        return;
    }
}

QSize QnCheckBoxedHeaderView::sectionSizeFromContents(int logicalIndex) const {
    QSize size = base_type::sectionSizeFromContents(logicalIndex);
    if (logicalIndex != CheckBoxColumn)
        return size;
    size.setWidth(15);
    return size;
}

void QnCheckBoxedHeaderView::at_sectionClicked(int logicalIndex) {
    if (logicalIndex != CheckBoxColumn)
        return;
    if (m_checkState != Qt::Checked)
        setCheckState(Qt::Checked);
    else
        setCheckState(Qt::Unchecked);
}


QnCameraAdditionDialog::QnCameraAdditionDialog(QWidget *parent):
    QDialog(parent),
    ui(new Ui::CameraAdditionDialog),
    m_server(NULL),
    m_inIpRangeEdit(false),
    m_subnetMode(false),
    m_inCheckStateChange(false)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::ManualCameraAddition_Help);

    m_header = new QnCheckBoxedHeaderView(this);
    ui->camerasTable->setHorizontalHeader(m_header);
    m_header->setVisible(true);
    m_header->setResizeMode(CheckBoxColumn, QHeaderView::ResizeToContents);
    m_header->setResizeMode(ManufColumn, QHeaderView::ResizeToContents);
    m_header->setResizeMode(NameColumn, QHeaderView::ResizeToContents);
    m_header->setResizeMode(UrlColumn, QHeaderView::Stretch);
    m_header->setClickable(true);

    connect(ui->startIPLineEdit,    SIGNAL(textChanged(QString)),                   this,   SLOT(at_startIPLineEdit_textChanged(QString)));
    connect(ui->startIPLineEdit,    SIGNAL(editingFinished()),                      this,   SLOT(at_startIPLineEdit_editingFinished()));
    connect(ui->endIPLineEdit,      SIGNAL(textChanged(QString)),                   this,   SLOT(at_endIPLineEdit_textChanged(QString)));
    connect(ui->camerasTable,       SIGNAL(cellChanged(int,int)),                   this,   SLOT(at_camerasTable_cellChanged(int, int)));
    connect(ui->camerasTable,       SIGNAL(cellClicked(int,int)),                   this,   SLOT(at_camerasTable_cellClicked(int, int)));
    connect(ui->subnetCheckbox,     SIGNAL(toggled(bool)),                          this,   SLOT(at_subnetCheckbox_toggled(bool)));
    connect(ui->closeButton,        SIGNAL(clicked()),                              this,   SLOT(accept()));
    connect(m_header,               SIGNAL(checkStateChanged(Qt::CheckState)),      this,   SLOT(at_header_checkStateChanged(Qt::CheckState)));
    connect(ui->portAutoCheckBox,   SIGNAL(toggled(bool)),                          ui->portSpinBox, SLOT(setDisabled(bool)));
    connect(qnResPool,              SIGNAL(resourceChanged(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceChanged(const QnResourcePtr &)));
    connect(qnResPool,              SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceRemoved(const QnResourcePtr &)));

    ui->scanProgressBar->setVisible(false);
    ui->stopScanButton->setVisible(false);
    ui->validateLabelSearch->setVisible(false);
    ui->serverOfflineLabel->setVisible(false);

    ui->cameraIpLineEdit->setMinimumSize(ui->startIPLineEdit->minimumSizeHint());

    ui->addButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

    connect(ui->scanButton, SIGNAL(clicked()), this, SLOT(at_scanButton_clicked()));
    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(at_addButton_clicked()));

    setWarningStyle(ui->validateLabelSearch);
    setWarningStyle(ui->serverOfflineLabel);

    updateSubnetMode();
    clearTable();
}

QnCameraAdditionDialog::~QnCameraAdditionDialog(){}

void QnCameraAdditionDialog::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
        return;

    clearTable();
    m_server = server;

    if (server) {
        setWindowTitle(tr("Add cameras to %1").arg(server->getName()));
        ui->validateLabelSearch->setVisible(false);
        ui->scanButton->setEnabled(true);
    } else {
        setWindowTitle(tr("Add cameras..."));
        ui->validateLabelSearch->setText(tr("Select target mediaserver in the tree."));
        ui->validateLabelSearch->setVisible(true);
        ui->scanButton->setEnabled(false);
    }
    ui->serverOfflineLabel->setVisible(server && server->getStatus() == QnResource::Offline);

    emit serverChanged();
}


void QnCameraAdditionDialog::clearTable() {
    ui->camerasTable->setRowCount(0);
    ui->camerasTable->setEnabled(false);
}

void QnCameraAdditionDialog::fillTable(const QnCamerasFoundInfoList &cameras) {
    clearTable();

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

        ui->camerasTable->setItem(row, CheckBoxColumn, checkItem);
        ui->camerasTable->setItem(row, ManufColumn, manufItem);
        ui->camerasTable->setItem(row, NameColumn, nameItem);
        ui->camerasTable->setItem(row, UrlColumn, urlItem);
    }
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);
}

void QnCameraAdditionDialog::removeAddedCameras() {
    int row = ui->camerasTable->rowCount() - 1;
    while (row >= 0){
        if (ui->camerasTable->item(row, CheckBoxColumn)->checkState() == Qt::Checked)
	        ui->camerasTable->removeRow(row);
		row--;
    }
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);
}

void QnCameraAdditionDialog::updateSubnetMode() {
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

bool QnCameraAdditionDialog::ensureServerOnline() {
    if (m_server && m_server->getStatus() != QnResource::Offline)
        return true;

    QMessageBox::critical(this,
                          tr("Error"),
                          tr("Server is offline.\n"\
                             "Camera addition is possible for online servers only."));
    return false;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnCameraAdditionDialog::at_startIPLineEdit_textChanged(QString value) {
    if (m_inIpRangeEdit)
        return;

    m_inIpRangeEdit = true;

    QString fixed(value);
    ui->startIPLineEdit->validator()->fixup(fixed);

    QHostAddress startAddr(fixed);
    ui->endIPLineEdit->setText(startAddr.toString());
    m_inIpRangeEdit = false;
}

void QnCameraAdditionDialog::at_startIPLineEdit_editingFinished() {
    QHostAddress startAddr(ui->endIPLineEdit->text());
    if (ui->subnetCheckbox->isChecked() && (startAddr.toIPv4Address() % 256 == 0)){
        QHostAddress endAddr(startAddr.toIPv4Address() + 255);
        ui->endIPLineEdit->setText(endAddr.toString());
    }
}

void QnCameraAdditionDialog::at_endIPLineEdit_textChanged(QString value) {
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

void QnCameraAdditionDialog::at_camerasTable_cellChanged( int row, int column) {
    Q_UNUSED(row)

    if (column > CheckBoxColumn)
        return;

    if (m_inCheckStateChange)
        return;

    m_inCheckStateChange = true;

    int rowCount = ui->camerasTable->rowCount();
    bool enabled = false;

    Qt::CheckState state = Qt::Unchecked;
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* item = ui->camerasTable->item(row, CheckBoxColumn);
        if (row == 0)
            state = item->checkState();

        if (item->checkState() != state)
            state = Qt::PartiallyChecked;

        if (item->checkState() == Qt::Checked)
            enabled = true;
    }
    ui->addButton->setEnabled(enabled);
    m_header->setCheckState(state);

    m_inCheckStateChange = false;
}

void QnCameraAdditionDialog::at_camerasTable_cellClicked(int row, int column) {
    if (column != UrlColumn)
        return;

    QUrl url(ui->camerasTable->item(row, column)->text());
    if (url.isEmpty())
        return;

    url.setPath(QString());
    QDesktopServices::openUrl(url);
}

void QnCameraAdditionDialog::at_header_checkStateChanged(Qt::CheckState state) {
    if (state == Qt::PartiallyChecked)
        return;

    if (m_inCheckStateChange)
        return;

    m_inCheckStateChange = true;
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* item = ui->camerasTable->item(row, CheckBoxColumn);
        item->setCheckState(state);
    }

    ui->addButton->setEnabled(rowCount > 0 && state == Qt::Checked);
    m_inCheckStateChange = false;
}

void QnCameraAdditionDialog::at_scanButton_clicked() {
    if (!ensureServerOnline())
        return;

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
        const QString& userInput = ui->cameraIpLineEdit->text();
        QUrl url = QUrl::fromUserInput(userInput);
        if (!url.isValid()) {
            ui->validateLabelSearch->setText(tr("Camera address field must contain valid url or ip address"));
            ui->validateLabelSearch->setVisible(true);
            return;
        }
        //startAddrStr = url.host();
        startAddrStr = userInput;
        endAddrStr = QString();
    }

    clearTable();
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

    QnConnectionRequestResult result;
    m_server->apiConnection()->searchCameraAsync(startAddrStr, endAddrStr, username, password, port, &result, SLOT(processReply(int, const QVariant &, int)));

    QEventLoop loop;
    connect(&result,            SIGNAL(replyProcessed()),   &loop, SLOT(quit()));
    connect(ui->stopScanButton, SIGNAL(clicked()),          &loop, SLOT(quit()));
    connect(ui->closeButton,    SIGNAL(clicked()),          &loop, SLOT(quit()));
    connect(this,               SIGNAL(serverChanged()),    &loop, SLOT(quit()));
    loop.exec();

    ui->scanButton->setEnabled(m_server);
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

    if(result.isFinished()) {
        if(result.status() == 0) {
            QnCamerasFoundInfoList cameras = result.reply().value<QnCamerasFoundInfoList>();

            if (cameras.size() > 0) {
                fillTable(cameras);
                ui->addButton->setFocus();
            } else {
                QMessageBox::information(this, tr("Finished"), tr("No cameras found"));
            }
        } else {
            if (!ensureServerOnline())
                return;

            QString error;
            if (0) { // TODO: #Elric
                error = tr("Could not connect to server.\nMake sure the server is available and try again.");
            } else {
                error = tr("Server returned an error."); 
            }
            
            QMessageBox::critical(this, tr("Error"), error);
        }
    }

    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);
    if (m_subnetMode)
        ui->startIPLineEdit->setFocus();
    else
        ui->cameraIpLineEdit->setFocus();
}

void QnCameraAdditionDialog::at_addButton_clicked() {
    if (!ensureServerOnline())
        return;

    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());

    QStringList urls;
    QStringList manufacturers;
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, CheckBoxColumn)->checkState() != Qt::Checked)
            continue;
        urls.append(ui->camerasTable->item(row, UrlColumn)->text());
        manufacturers.append(ui->camerasTable->item(row, ManufColumn)->text());
    }
    if (urls.empty()){
        QMessageBox::information(this, tr("No cameras selected"), tr("Please select at least one camera"));
        return;
    }

    ui->addButton->setEnabled(false);
    ui->scanButton->setEnabled(false);
    ui->camerasTable->setEnabled(false);

    QnConnectionRequestResult result;
    m_server->apiConnection()->addCameraAsync(urls, manufacturers, username, password, &result, SLOT(processReply(int, const QVariant &, int)));

    QEventLoop loop;
    connect(&result,            SIGNAL(replyProcessed()),   &loop, SLOT(quit()));
    connect(ui->closeButton,    SIGNAL(clicked()),          &loop, SLOT(quit()));
    connect(this,               SIGNAL(serverChanged()),    &loop, SLOT(quit()));
    loop.exec();

    ui->addButton->setEnabled(true);
    ui->scanButton->setEnabled(m_server);
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);

    if(result.isFinished()) {
        if(result.status() == 0) {
            removeAddedCameras();
            QMessageBox::information(
                this,
                tr("Success"),
                tr("%n camera(s) added successfully.\nIt might take a few moments to populate them in the tree.", "", urls.size()),
                QMessageBox::Ok
            );
        } else {
            if (!ensureServerOnline())
                return;
            QMessageBox::critical(this, tr("Error"), tr("Error while adding camera(s)", "", urls.size()));
        }
    }
}

void QnCameraAdditionDialog::at_subnetCheckbox_toggled(bool toggled) {
    if (m_subnetMode == toggled)
        return;

    m_subnetMode = toggled;
    updateSubnetMode();
}

void QnCameraAdditionDialog::at_resPool_resourceChanged(const QnResourcePtr &resource) {
    if (resource != m_server)
        return;
    ui->serverOfflineLabel->setVisible(resource->getStatus() == QnResource::Offline);
}

void QnCameraAdditionDialog::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (resource != m_server)
        return;
    setServer(QnMediaServerResourcePtr());
}

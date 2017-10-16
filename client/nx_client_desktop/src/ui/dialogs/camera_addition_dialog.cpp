#include "camera_addition_dialog.h"
#include "ui_camera_addition_dialog.h"

#include <QtGui/QStandardItemModel>
#include <QtGui/QDesktopServices>

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <ui/common/aligner.h>
#include <ui/style/custom_style.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/views/checkboxed_header_view.h>
#include <ui/workbench/workbench_context.h>

namespace {

enum Column
{
    CheckBoxColumn,
    ManufColumn,
    NameColumn,
    UrlColumn
};

const int kAutoPort = 0;

const QString kServerNameFormat = QString::fromLatin1(
    "<span style='font-weight: 400; font-size: 24px; color: %3'>%1 </span>"
    "<span style='font-weight: 250; font-size: 18px; color: %4'>%2 </span>");

} // namespace

QnCameraAdditionDialog::QnCameraAdditionDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraAdditionDialog),
    m_state(NoServer),
    m_server(NULL),
    m_inIpRangeEdit(false),
    m_subnetMode(false),
    m_inCheckStateChange(false)
{
    ui->setupUi(this);

    const auto examples = QString::fromLatin1(R"(
        <html>%1<b><ul>
        <li>192.168.1.15</li>
        <li>www.example.com:8080</li>
        <li>http://example.com:7090/image.jpg</li>
        <li>rtsp://example.com:554/video</li>
        <li>udp://239.250.5.5:1234</li>
        </ul></b></html>
        )")
        .arg(tr("Examples:"));

    ui->singleCameraLineEdit->setToolTip(examples);

    setHelpTopic(this, Qn::ManualCameraAddition_Help);

    auto scrollBar = new QnSnappedScrollBar(Qt::Vertical, this);
    scrollBar->setUseItemViewPaddingWhenVisible(false);
    scrollBar->setUseMaximumSpace(true);
    ui->camerasTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    m_header = new QnCheckBoxedHeaderView(CheckBoxColumn, this);
    ui->camerasTable->setHorizontalHeader(m_header);
    m_header->setVisible(true);
    m_header->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_header->setSectionResizeMode(UrlColumn, QHeaderView::Stretch);
    m_header->setSectionsClickable(true);

    connect(ui->startIPLineEdit,    SIGNAL(textChanged(QString)),                   this,   SLOT(at_startIPLineEdit_textChanged(QString)));
    connect(ui->startIPLineEdit,    SIGNAL(editingFinished()),                      this,   SLOT(at_startIPLineEdit_editingFinished()));
    connect(ui->endIPLineEdit,      SIGNAL(textChanged(QString)),                   this,   SLOT(at_endIPLineEdit_textChanged(QString)));
    connect(ui->camerasTable,       SIGNAL(cellChanged(int,int)),                   this,   SLOT(at_camerasTable_cellChanged(int, int)));
    connect(ui->camerasTable,       SIGNAL(cellClicked(int,int)),                   this,   SLOT(at_camerasTable_cellClicked(int, int)));
    connect(ui->subnetCheckbox,     SIGNAL(toggled(bool)),                          this,   SLOT(at_subnetCheckbox_toggled(bool)));
    connect(ui->scanButton,         SIGNAL(clicked()),                              this,   SLOT(at_scanButton_clicked()));
    connect(ui->stopScanButton,     SIGNAL(clicked()),                              this,   SLOT(at_stopScanButton_clicked()));
    connect(ui->addButton,          SIGNAL(clicked()),                              this,   SLOT(at_addButton_clicked()));
    connect(ui->backToScanButton,   SIGNAL(clicked()),                              this,   SLOT(at_backToScanButton_clicked()));
    connect(m_header,               SIGNAL(checkStateChanged(Qt::CheckState)),      this,   SLOT(at_header_checkStateChanged(Qt::CheckState)));
    connect(ui->portAutoCheckBox,   SIGNAL(toggled(bool)),                          this,   SLOT(at_portAutoCheckBox_toggled(bool)));
    connect(resourcePool(),              SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resPool_resourceRemoved(const QnResourcePtr &)));

    setWarningStyle(ui->validateLabelSearch);
    ui->validateLabelSearch->setVisible(false);

    setWarningStyle(ui->serverOfflineLabel);
    ui->serverOfflineLabel->setVisible(false);

    setWarningStyle(ui->serverStatusLabel);
    ui->serverStatusLabel->setVisible(false);

    ui->progressWidget->setVisible(false);

    auto aligner = new QnAligner(this);
    aligner->addWidgets({
        ui->singleCameraLabel,
        ui->startIPLabel,
        ui->endIPLabel });
}

QnCameraAdditionDialog::~QnCameraAdditionDialog(){
}

QnMediaServerResourcePtr QnCameraAdditionDialog::server() const {
    return m_server;
}

void QnCameraAdditionDialog::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
        return;

    if (m_server)
        disconnect(m_server, NULL, this, NULL);

    m_server = server;
    if (server) {
        setState(server->getStatus() == Qn::Offline
                 ? InitialOffline
                 : Initial);

        connect(m_server, &QnResource::statusChanged, this, &QnCameraAdditionDialog::at_server_statusChanged);
        connect(m_server, &QnResource::nameChanged,  this, &QnCameraAdditionDialog::updateTitle);
        connect(m_server, &QnResource::urlChanged,  this, &QnCameraAdditionDialog::updateTitle);
    } else {
        setState(NoServer);
    }
    clearServerStatus();
    updateTitle();
}

void QnCameraAdditionDialog::reject() {
    if (!tryClose(false))
        return;
    base_type::reject();
}

QnCameraAdditionDialog::State QnCameraAdditionDialog::state() const {
    return m_state;
}

void QnCameraAdditionDialog::setState(QnCameraAdditionDialog::State state) {
    if (m_state == state)
        return;

    m_state = state;

    ui->validateLabelSearch->setVisible(false);         // hide on every state change
    ui->progressWidget->setVisible(m_state == Searching || m_state == Stopping);
    ui->scanParamsWidget->setEnabled(m_state == Initial);
    ui->serverOfflineLabel->setVisible(m_state == InitialOffline || m_state == CamerasOffline);

    switch (m_state) {
    case NoServer:
        ui->stageStackedWidget->setCurrentWidget(ui->searchParametersPage);
        ui->stageStackedWidget->setEnabled(false);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->scanButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        clearTable();
        break;
    case Initial:
        ui->stageStackedWidget->setCurrentWidget(ui->searchParametersPage);
        ui->stageStackedWidget->setEnabled(true);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->scanButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(true);
        ui->scanButton->setFocus();
        ui->scanButton->setDefault(true);
        ui->scanButton->setEnabled(true);
        clearTable();
        break;
    case InitialOffline:
        ui->stageStackedWidget->setCurrentWidget(ui->searchParametersPage);
        ui->stageStackedWidget->setEnabled(false);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->scanButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        ui->scanButton->setEnabled(false);
        clearTable();
        break;
    case Searching:
        ui->progressBar->setFormat(lit("%1\t").arg(tr("Initializing scan...")));
        ui->progressBar->setMaximum(1);
        ui->progressBar->setValue(0);
        ui->stageStackedWidget->setCurrentWidget(ui->discoveredCamerasPage);
        ui->stageStackedWidget->setEnabled(true);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->addButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(false);
        ui->stopScanButton->setEnabled(false);
        ui->camerasTable->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        clearTable();
        break;
    case Stopping:
        ui->stopScanButton->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        break;
    case CamerasFound:
    {
        ui->stageStackedWidget->setCurrentWidget(ui->discoveredCamerasPage);
        ui->stageStackedWidget->setEnabled(true);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->addButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(true);

        bool camerasFound = ui->camerasTable->rowCount() > 0;
        ui->camerasTable->setEnabled(camerasFound);
        bool canAdd = addingAllowed();
        ui->addButton->setEnabled(canAdd);
        if (canAdd)
            ui->addButton->setFocus();
        else
            ui->backToScanButton->setFocus();
        break;
    }
    case CamerasOffline:
        ui->stageStackedWidget->setCurrentWidget(ui->discoveredCamerasPage);
        ui->stageStackedWidget->setEnabled(true);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->addButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        break;
    case Adding:
        ui->stageStackedWidget->setCurrentWidget(ui->discoveredCamerasPage);
        ui->stageStackedWidget->setEnabled(false);
        ui->actionButtonsStackedWidget->setCurrentWidget(ui->addButtonPage);
        ui->actionButtonsStackedWidget->setEnabled(false);
        ui->buttonBox->button(QDialogButtonBox::Close)->setFocus();
        break;
    default:
        break;
    }
}

void QnCameraAdditionDialog::clearTable() {
    ui->camerasTable->setRowCount(0);
}

int QnCameraAdditionDialog::fillTable(const QnManualResourceSearchList &cameras) {
    clearTable();

    int newCameras = 0;
    foreach(const QnManualResourceSearchEntry &info, cameras) {
        bool enabledRow = !info.existsInPool;

        //insert new cameras to the beginning, old to the end
        int row = enabledRow ? newCameras : ui->camerasTable->rowCount();
        ui->camerasTable->insertRow(row);

        if (enabledRow)
            newCameras++;

        QTableWidgetItem *checkItem = new QTableWidgetItem();
        checkItem->setFlags(checkItem->flags() | Qt::ItemIsUserCheckable);
        if (enabledRow) {
            checkItem->setCheckState(Qt::Checked);
        } else {
            checkItem->setFlags(checkItem->flags() | Qt::ItemIsTristate);
            checkItem->setFlags(checkItem->flags() &~ Qt::ItemIsEnabled);
            checkItem->setCheckState(Qt::PartiallyChecked);
        }
        checkItem->setData(Qt::UserRole, qVariantFromValue<QnManualResourceSearchEntry>(info));

        QTableWidgetItem *manufItem = new QTableWidgetItem(info.vendor);
        manufItem->setFlags(manufItem->flags() &~ Qt::ItemIsEditable);
        if (!enabledRow)
            manufItem->setFlags(manufItem->flags() &~ Qt::ItemIsEnabled);

        QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
        nameItem->setFlags(nameItem->flags() &~ Qt::ItemIsEditable);
        if (!enabledRow)
            nameItem->setFlags(nameItem->flags() &~ Qt::ItemIsEnabled);

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
    return newCameras;
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

void QnCameraAdditionDialog::updateSubnetMode()
{
    ui->addressStackedWidget->setCurrentWidget(m_subnetMode
                                               ? ui->pageSubnet
                                               : ui->pageSingleCamera);
    if (m_subnetMode)
    {
        ui->startIPLineEdit->setFocus();
        if (!ui->singleCameraLineEdit->text().isEmpty())
        {
            ui->startIPLineEdit->setText(ui->singleCameraLineEdit->text());
            QHostAddress startAddr(ui->startIPLineEdit->text());
            if (startAddr.toIPv4Address())
            {
                ui->startIPLineEdit->setText(startAddr.toString());
                quint32 addr = startAddr.toIPv4Address();
                addr = addr >> 8;
                addr = (addr << 8) + 255;
                QString endAddrStr = QHostAddress(addr).toString();
                ui->endIPLineEdit->setText(endAddrStr);
                ui->endIPLineEdit->setFocus();
                ui->endIPLineEdit->setSelection(endAddrStr.size() - 3, 3);
            }
        }
    }
    else
    {
        ui->singleCameraLineEdit->setText(ui->startIPLineEdit->text());
        ui->singleCameraLineEdit->setFocus();
    }
}

bool QnCameraAdditionDialog::serverOnline() const {
    return m_server && m_server->getStatus() != Qn::Offline;
}

bool QnCameraAdditionDialog::ensureServerOnline()
{
    if (serverOnline())
        return true;

    QnMessageBox::critical(this, tr("Server offline"),
        tr("Device adding is possible for online servers only."));
    return false;
}

bool QnCameraAdditionDialog::addingAllowed() const {
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* item = ui->camerasTable->item(row, CheckBoxColumn);
        if (!(item->flags() & Qt::ItemIsEnabled))
            continue;
        if (item->checkState() == Qt::Checked)
            return true;
    }
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

void QnCameraAdditionDialog::at_camerasTable_cellChanged(int row, int column) {
    Q_UNUSED(row)

    if (column > CheckBoxColumn)
        return;

    if (m_inCheckStateChange)
        return;

    m_inCheckStateChange = true;

    int rowCount = ui->camerasTable->rowCount();
    bool enabled = false;

    Qt::CheckState state = Qt::Unchecked;
    bool firstRow =  true;
    for (int row = 0; row < rowCount; ++row) {
        QTableWidgetItem* item = ui->camerasTable->item(row, CheckBoxColumn);
        if (!(item->flags() & Qt::ItemIsEnabled))
            continue;

        if (firstRow)
            state = item->checkState();
        firstRow = false;

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

    QnManualResourceSearchEntry info = ui->camerasTable->item(row, CheckBoxColumn)->data(Qt::UserRole).value<QnManualResourceSearchEntry>();

    QUrl url = QUrl::fromUserInput(info.url);
    if (!url.isValid())
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
    int enabledRowCount = 0;
    for (int row = 0; row < ui->camerasTable->rowCount(); ++row) {
        QTableWidgetItem* item = ui->camerasTable->item(row, CheckBoxColumn);
        if (!(item->flags() & Qt::ItemIsEnabled))
            continue;
        item->setCheckState(state);
        enabledRowCount++;
    }

    ui->addButton->setEnabled(enabledRowCount > 0 && state == Qt::Checked);
    m_inCheckStateChange = false;
}

void QnCameraAdditionDialog::at_scanButton_clicked() {
    if (!ensureServerOnline())
        return;

    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());
    int port = ui->portAutoCheckBox->isChecked()
            ? kAutoPort
            : ui->portSpinBox->value();

    QString startAddrStr;
    QString endAddrStr;
    if (m_subnetMode) {
        startAddrStr = ui->startIPLineEdit->text().simplified();
        endAddrStr = ui->endIPLineEdit->text().simplified();

        QHostAddress startAddr(startAddrStr);
        QHostAddress endAddr(endAddrStr);

        if (startAddr.toIPv4Address() > endAddr.toIPv4Address()){
            ui->validateLabelSearch->setText(tr("First address in range is greater than the last one."));
            ui->validateLabelSearch->setVisible(true);
            return;
        }

        if (!endAddr.isInSubnet(startAddr, 8)){
            ui->validateLabelSearch->setText(tr("The specified IP address range has more than 255 addresses."));
            ui->validateLabelSearch->setVisible(true);
            return;
        }
    } else {
        QString userInput = ui->singleCameraLineEdit->text().simplified();
        QUrl url = QUrl::fromUserInput(userInput);
        if (!url.isValid()) {
            ui->validateLabelSearch->setText(tr("Device address field must contain a valid URL, IP address, or RTSP link."));
            ui->validateLabelSearch->setVisible(true);
            return;
        }
        startAddrStr = userInput;
        endAddrStr = QString();
    }

    setState(Searching);
    m_processUuid = QnUuid();
    m_server->apiConnection()->searchCameraAsyncStart(startAddrStr, endAddrStr, username, password, port, this, SLOT(at_searchRequestReply(int, const QVariant &, int)));
}

void QnCameraAdditionDialog::at_stopScanButton_clicked() {
    if (m_state != Searching)
        return;
    setState(Stopping);

    // init stage, cannot stop the process
    if (m_processUuid.isNull())
        return; // TODO: #GDM #CameraAddition do something

    m_server->apiConnection()->searchCameraAsyncStop(m_processUuid, this, SLOT(at_searchRequestReply(int, const QVariant &, int)));
    ui->progressBar->setFormat(lit("%1\t").arg(tr("Finishing searching...")));
    ui->progressBar->setMaximum(1);
    ui->progressBar->setValue(0);
    m_processUuid = QnUuid();
}

void QnCameraAdditionDialog::at_addButton_clicked() {
    if (!ensureServerOnline())
        return;

    QString username(ui->loginLineEdit->text());
    QString password(ui->passwordLineEdit->text());

    QnManualResourceSearchList camerasToAdd;
    int rowCount = ui->camerasTable->rowCount();
    for (int row = 0; row < rowCount; ++row) {
        if (ui->camerasTable->item(row, CheckBoxColumn)->checkState() != Qt::Checked)
            continue;

        QnManualResourceSearchEntry info = ui->camerasTable->item(row, CheckBoxColumn)->data(Qt::UserRole).value<QnManualResourceSearchEntry>();
        camerasToAdd << info;
    }

    if (camerasToAdd.empty())
    {
        QnMessageBox::information(this,
            tr("No devices selected."), tr("Please select at least one device"));
        return;
    }

    QnConnectionRequestResult result;
    m_server->apiConnection()->addCameraAsync(camerasToAdd, username, password, &result, SLOT(processReply(int, const QVariant &, int)));
    setState(Adding);

    QEventLoop loop;
    connect(&result,            &QnConnectionRequestResult::replyProcessed, &loop, &QEventLoop::quit);
    connect(ui->buttonBox,      &QDialogButtonBox::rejected,                &loop, &QEventLoop::quit);
    loop.exec();

    ui->addButton->setEnabled(addingAllowed());
    ui->scanButton->setEnabled(m_server);
    ui->camerasTable->setEnabled(ui->camerasTable->rowCount() > 0);

    if(result.isFinished())
    {
        if(result.status() == 0)
        {
            removeAddedCameras();
            QnMessageBox::success(this,
                tr("%n devices added.", "", camerasToAdd.size()),
                tr("It might take them a few moments to appear."));
        }
        else
        {
            if (!ensureServerOnline())
            {
                setState(CamerasOffline);
                return;
            }
            QnMessageBox::critical(this, tr("Failed to add %n devices", "", camerasToAdd.size()));
        }
    }
    setState(CamerasFound);
}

void QnCameraAdditionDialog::at_backToScanButton_clicked() {
    if (!m_server) {
        setState(NoServer);
        return;
    }

    setState(m_server->getStatus() == Qn::Offline
             ? InitialOffline
             : Initial);
}

void QnCameraAdditionDialog::at_subnetCheckbox_toggled(bool toggled) {
    if (m_subnetMode == toggled)
        return;

    m_subnetMode = toggled;
    updateSubnetMode();
}

void QnCameraAdditionDialog::at_portAutoCheckBox_toggled(bool toggled) {
    ui->portStackedWidget->setCurrentWidget(toggled ? ui->pageAuto : ui->pagePort);
}

void QnCameraAdditionDialog::at_server_statusChanged(const QnResourcePtr &resource) {
    if (resource != m_server)
        return;

    clearServerStatus();
    if (resource->getStatus() == Qn::Offline) {
        switch (m_state) {
        case Initial:
            setState(InitialOffline);
            break;
        case Searching:
            setState(InitialOffline);
            updateServerStatus(tr("Server went offline - search aborted."));
            break;
        case CamerasFound:
        case Adding:
            setState(CamerasOffline);
            updateServerStatus(tr("Server is offline, devices can only be added to an online server."));
            break;
        default:
            break;
        }
    } else {
        switch (m_state) {
        case InitialOffline:
            setState(Initial);
            break;
        case CamerasOffline:
            setState(CamerasFound);
            break;
        default:
            break;
        }
    }
}

void QnCameraAdditionDialog::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    if (resource != m_server)
        return;
    State oldState = m_state;
    setServer(QnMediaServerResourcePtr());
    switch (oldState) {
    case Searching:
        updateServerStatus(tr("Server has been removed - search aborted."));
        break;
    case CamerasFound:
    case Adding:
        updateServerStatus(tr("Server has been removed - cannot add devices."));
        break;
    default:
        break;
    }
}

void QnCameraAdditionDialog::at_searchRequestReply(int status, const QVariant &reply, int handle) {
    Q_UNUSED(handle)

    if (m_state != Searching && m_state != Stopping)
        return;

    if (status != 0) {
        setState(Initial);
        QnMessageBox::critical(this, tr("Device search failed"));
        return;
    }

    QnManualCameraSearchReply result = reply.value<QnManualCameraSearchReply>(); // TODO: use this type in slot signature.

    int newCameras = fillTable(result.cameras);

    switch (result.status.state) {
    case QnManualResourceSearchStatus::Init:
        if (m_state == Searching)
            ui->progressBar->setFormat(lit("%1\t").arg(tr("Initializing scan...")));
        break;
    case QnManualResourceSearchStatus::CheckingOnline:
        if (m_state == Searching)
            ui->progressBar->setFormat(lit("%1\t").arg(tr("Scanning online hosts...")));
        break;
    case QnManualResourceSearchStatus::CheckingHost:
        if (m_state == Searching) {
            const QString found = tr("%n devices found", "", result.cameras.size());
            if (m_subnetMode)
                //: Scanning hosts... (5 devices found)
                ui->progressBar->setFormat(lit("%1\t(%2)").arg(tr("Scanning hosts...")).arg(found));
            else
                //: Scanning host... (0 devices found)
                ui->progressBar->setFormat(lit("%1\t(%2)").arg(tr("Scanning host...")).arg(found));
        }
        break;
    case QnManualResourceSearchStatus::Finished:
    case QnManualResourceSearchStatus::Aborted:
        if (m_state == Searching)
            m_server->apiConnection()->searchCameraAsyncStop(m_processUuid); //clear server cache

        if (result.cameras.size() > 0)
        {
            setState(CamerasFound);
            if (newCameras == 0)
                QnMessageBox::information(this, tr("All devices already added"));
        }
        else
        {
            setState(Initial);
            QnMessageBox::warning(this, tr("No devices found"));
        }
        m_processUuid = QnUuid();
    }

    if (m_state != Searching)
        return;

    if (m_processUuid.isNull()) {
        m_processUuid = result.processUuid;
        ui->stopScanButton->setEnabled(true);
        ui->stopScanButton->setFocus();
    }
    ui->progressBar->setMaximum(result.status.total);
    ui->progressBar->setValue(result.status.current);

    static const int PROGRESS_CHECK_PERIOD_MS = 1000;
    QTimer::singleShot(PROGRESS_CHECK_PERIOD_MS, this, SLOT(updateStatus()));
}

void QnCameraAdditionDialog::updateStatus() {
    if (m_state == Searching)
        m_server->apiConnection()->searchCameraAsyncStatus(m_processUuid, this, SLOT(at_searchRequestReply(int, const QVariant &, int)));
}

bool QnCameraAdditionDialog::tryClose(bool force) {
    if (m_server && !m_processUuid.isNull())
        m_server->apiConnection()->searchCameraAsyncStop(m_processUuid, NULL, NULL);
    setState(Initial);
    if (force)
        hide();
    return true;
}

void QnCameraAdditionDialog::updateTitle()
{
    if (m_server)
    {
        QnResourceDisplayInfo info(m_server);
        setWindowTitle(tr("Add devices to %1").arg(info.name()));
        ui->serverNameLabel->setText(kServerNameFormat.
            arg(info.name()).
            arg(info.extraInfo()).
            arg(palette().color(QPalette::Text).name()).
            arg(palette().color(QPalette::WindowText).name()));
    }
    else
    {
        setWindowTitle(tr("Add devices..."));
        ui->serverNameLabel->setText(tr("Select target server..."));
    }
}


void QnCameraAdditionDialog::clearServerStatus() {
    updateServerStatus(QString());
}

void QnCameraAdditionDialog::updateServerStatus(const QString &errorMessage) {
    ui->serverStatusLabel->setText(errorMessage);
    ui->serverStatusLabel->setVisible(!errorMessage.isEmpty());
}


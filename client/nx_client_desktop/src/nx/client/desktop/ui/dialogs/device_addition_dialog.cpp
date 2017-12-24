#include "device_addition_dialog.h"
#include "ui_device_addition_dialog.h"

#include "private/found_devices_model.h"
#include "private/manual_device_searcher.h"
#include "private/presented_state_delegate.h"

#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>

#include <core/resource/media_server_resource.h>
#include <utils/common/event_processors.h>

namespace {

bool isKnownAddressPage(QTabWidget* tabWidget)
{
    static constexpr auto kKnownAddressPageIndex = 0;
    return tabWidget->currentIndex() == kKnownAddressPageIndex;
}

} // namespace

namespace nx {
namespace client {
namespace desktop {
namespace ui {

DeviceAdditionDialog::DeviceAdditionDialog(QWidget* parent):
    base_type(parent),
    m_serversWatcher(parent),
    m_serverStatusWatcher(this),
    ui(new Ui::DeviceAdditionDialog())
{
    ui->setupUi(this);

    initializeControls();
}

DeviceAdditionDialog::~DeviceAdditionDialog()
{
    stopSearch();
}

void DeviceAdditionDialog::initializeControls()
{
    //FIXME: remove default address
    ui->addressEdit->setText(lit("rtsp://media.smart-streaming.com/mytest/mp4:sample_phone_150k.mp4"));
    connect(ui->searchButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::handleStartSearchClicked);
    connect(ui->stopSearchButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::stopSearch);
    connect(ui->addDevicesButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::handleAddDevicesClicked);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
        this, &DeviceAdditionDialog::handleSearchTypeChanged);

    const auto tabWidgetSizePolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    autoResizePagesToContents(ui->tabWidget, tabWidgetSizePolicy, true);
    setTabShape(ui->tabWidget->tabBar(), style::TabShape::Compact);

    connect(&m_serversWatcher, &SystemServersWatcher::serverAdded,
        ui->selectServerMenuButton, &QnChooseServerButton::addServer);
    connect(&m_serversWatcher, &SystemServersWatcher::serverRemoved,
        ui->selectServerMenuButton, &QnChooseServerButton::removeServer);
    for (const auto server: m_serversWatcher.servers())
        ui->selectServerMenuButton->addServer(server);

    installEventHandler(ui->serverChoosePanel, QEvent::PaletteChange, ui->serverChoosePanel,
        [this]()
        {
            setPaletteColor(ui->serverChoosePanel, QPalette::Background,
                palette().color(QPalette::Mid));
        });

    connect(ui->selectServerMenuButton, &QnChooseServerButton::currentServerChanged,
        this, &DeviceAdditionDialog::handleSelectedServerChanged);

    ui->serverOfflineAlertBar->setText(tr("Server offline"));

    updateSelectedServerButtonVisibility();
    connect(&m_serversWatcher, &SystemServersWatcher::serversCountChanged,
        this, &DeviceAdditionDialog::updateSelectedServerButtonVisibility);
    connect(&m_serverStatusWatcher, &ServerOnlineStatusWatcher::statusChanged,
        this, &DeviceAdditionDialog::handleServerOnlineStateChanged);
    ui->serverOfflineAlertBar->setVisible(false);

    connect(this, &DeviceAdditionDialog::rejected,
        this, &DeviceAdditionDialog::handleDialogClosed);

    setupTable();
    setupPortStuff(ui->knownAddressAutoPortCheckBox, ui->knownAddressPortSpinBox);
    setupPortStuff(ui->subnetScanAutoPortCheckBox, ui->subnetScanPortSpinBox);

    setAccentStyle(ui->searchButton);
    setAccentStyle(ui->addDevicesButton);

    updateResultsWidgetState();
}

void DeviceAdditionDialog::updateSelectedServerButtonVisibility()
{
    const bool visible = m_serversWatcher.serversCount() > 1;
    ui->serverChoosePanel->setVisible(visible);
}

void DeviceAdditionDialog::handleSelectedServerChanged(const QnMediaServerResourcePtr& previous)
{
    m_serverStatusWatcher.setServer(ui->selectServerMenuButton->currentServer());
    if (!previous)
        return;

    cleanUnfinishedSearches(previous);
    stopSearch();
}

void DeviceAdditionDialog::handleServerOnlineStateChanged()
{
    const bool online = m_serverStatusWatcher.isOnline();

    ui->serverOfflineAlertBar->setVisible(!online);
    ui->searchResultsStackedWidget->setEnabled(online);
    ui->searchPanel->setEnabled(online);

    if (!online && m_currentSearch)
        stopSearch();
}

void DeviceAdditionDialog::setupTable()
{
    const auto table = ui->foundDevicesTable;

    table->setCheckboxColumn(FoundDevicesModel::checkboxColumn, true);
    table->setPersistentDelegateForColumn(FoundDevicesModel::presentedStateColumn,
        new PresentedStateDelegate(table));
}

void DeviceAdditionDialog::setupTableHeader()
{
    const auto header = ui->foundDevicesTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(
        FoundDevicesModel::addressColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        FoundDevicesModel::checkboxColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        FoundDevicesModel::presentedStateColumn, QHeaderView::ResizeToContents);
}

void DeviceAdditionDialog::setupPortStuff(QCheckBox* autoCheckbox, QSpinBox* portSpinBox)
{
    connect(autoCheckbox, &QCheckBox::toggled, this,
        [portSpinBox](bool checked)
        {
            portSpinBox->setEnabled(!checked);
        });
}

void DeviceAdditionDialog::setServer(const QnMediaServerResourcePtr& value)
{
    if (!ui->selectServerMenuButton->setCurrentServer(value))
        stopSearch();
}

int DeviceAdditionDialog::port() const
{
    const bool isKnownAddressTabPage = isKnownAddressPage(ui->tabWidget);
    const auto portSpinBox = isKnownAddressTabPage
        ? ui->knownAddressPortSpinBox
        : ui->subnetScanPortSpinBox;
    const auto autoPortSpinBox = isKnownAddressTabPage
        ? ui->knownAddressAutoPortCheckBox
        : ui->subnetScanAutoPortCheckBox;

    static constexpr auto kAutoPortValue = 0;
    return autoPortSpinBox->isChecked()
        ? kAutoPortValue
        : portSpinBox->value();
}

QString DeviceAdditionDialog::password() const
{
    const auto passwordEdit = isKnownAddressPage(ui->tabWidget)
        ? ui->knownAddressPasswordEdit
        : ui->subnetScanPasswordEdit;

    return passwordEdit->text().trimmed();
}

QString DeviceAdditionDialog::login() const
{
    const auto loginEdit = isKnownAddressPage(ui->tabWidget)
        ? ui->knownAddressLoginEdit
        : ui->subnetScanLoginEdit;

    return loginEdit->text().trimmed();
}

void DeviceAdditionDialog::handleStartSearchClicked()
{
    if (m_currentSearch)
        stopSearch();

    const bool isKnownAddressTabPage = isKnownAddressPage(ui->tabWidget);
    if (isKnownAddressTabPage)
    {
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->addressEdit->text().simplified(), login(), password(), port()));
    }
    else
    {
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->startAddressEdit->text().trimmed(), ui->endAddressEdit->text().trimmed(),
            login(), password(), port()));
    }

    if (m_currentSearch->progress() == QnManualResourceSearchStatus::Aborted)
    {
        // FIXME: add error message handling
        qDebug() << "----- ERROR: " << m_currentSearch->lastErrorText();
        stopSearch();
        return;
    }

    resetButtonStyle(ui->searchButton);

    m_model.reset(new FoundDevicesModel());

    updateResultsWidgetState();

    connect(m_currentSearch, &ManualDeviceSearcher::progressChanged,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model, &FoundDevicesModel::rowCountChanged,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model, &FoundDevicesModel::dataChanged,
        this, &DeviceAdditionDialog::handleModelDataChanged);

    ui->foundDevicesTable->setModel(m_model.data());
    setupTableHeader();
    updateAddDevicesButtonText();

    connect(m_currentSearch, &ManualDeviceSearcher::devicesAdded,
        m_model, &FoundDevicesModel::addDevices);
    connect(m_currentSearch, &ManualDeviceSearcher::devicesRemoved,
        m_model, &FoundDevicesModel::removeDevices);
}

void DeviceAdditionDialog::handleAddDevicesClicked()
{
    const auto server = ui->selectServerMenuButton->currentServer();
    if (!server || server->getStatus() != Qn::Online || !m_currentSearch || !m_model)
        return;

    QnManualResourceSearchList devices;

    QHash<QString, FoundDevicesModel::PresentedState> prevStates;

    const bool checkSelection = ui->foundDevicesTable->getCheckedCount();
    const int rowsCount = m_model->rowCount();
    for (int row = 0; row != rowsCount; ++row)
    {
        const auto choosenIndex = m_model->index(row, FoundDevicesModel::checkboxColumn);
        if (checkSelection && !m_model->data(choosenIndex, Qt::CheckStateRole).toBool())
            continue;

        const auto dataIndex = m_model->index(row);
        const auto device = m_model->data(dataIndex, FoundDevicesModel::dataRole)
            .value<QnManualResourceSearchEntry>();

        const auto stateIndex = m_model->index(row, FoundDevicesModel::presentedStateColumn);
        prevStates[device.uniqueId] = stateIndex.data(FoundDevicesModel::presentedStateRole)
            .value<FoundDevicesModel::PresentedState>();
        m_model->setData(stateIndex, FoundDevicesModel::addingInProgressState,
            FoundDevicesModel::presentedStateRole);

        devices.append(device);
    }

    if (devices.isEmpty())
        return;

    QnConnectionRequestResult result;
    server->apiConnection()->addCameraAsync(devices,
        m_currentSearch->login(), m_currentSearch->password(),
        &result, SLOT(processReply(int, const QVariant &, int)));

    QEventLoop loop;
    connect(&result, &QnConnectionRequestResult::replyProcessed, &loop, &QEventLoop::quit);
    connect(this, &DeviceAdditionDialog::rejected, &loop, &QEventLoop::quit);


    const auto connection =
        connect(&m_serverStatusWatcher, &ServerOnlineStatusWatcher::statusChanged, this,
            [this, &loop]()
            {
                if (!m_serverStatusWatcher.isOnline())
                    loop.quit();
            });

    loop.exec();
    disconnect(connection);

    if (result.status() == 0)
    {
        for (auto& state: prevStates)
            state = FoundDevicesModel::alreadyAddedState;
        qDebug() << "Added!";
    }
    else
    {
        qDebug() << "Failed!";
    }

    for (auto it = prevStates.begin(); it != prevStates.end(); ++it)
    {
        const auto id = it.key();
        const auto state = it.value();
        const auto index = m_model->indexByUniqueId(
            id, FoundDevicesModel::presentedStateColumn);
        m_model->setData(index, state, FoundDevicesModel::presentedStateRole);
    }
}

void DeviceAdditionDialog::stopSearch()
{
    if (!m_currentSearch)
        return;

    m_currentSearch->disconnect(this);
    m_currentSearch->disconnect(m_model);

    if (m_currentSearch->searching())
    {
        m_currentSearch->stop();
        m_unfinishedSearches.append(m_currentSearch);

        // Next state may be only "Finished".
        connect(m_currentSearch, &ManualDeviceSearcher::progressChanged, this,
            [this, searcher = m_currentSearch]()
            {
                qDebug() << searcher->progress();
                if (!searcher->searching())
                {
                    searcher->disconnect(this);
                    removeUnfinishedSearch(searcher);
                }
            });
    }
    m_currentSearch.reset();
    updateResultsWidgetState();
}

void DeviceAdditionDialog::cleanUnfinishedSearches(const QnMediaServerResourcePtr& server)
{
    const auto newEnd = std::remove_if(m_unfinishedSearches.begin(), m_unfinishedSearches.end(),
        [server](const SearcherPtr& searcher)
        {
            return searcher->server() == server;
        });
    m_unfinishedSearches.erase(newEnd, m_unfinishedSearches.end());
}

void DeviceAdditionDialog::removeUnfinishedSearch(const SearcherPtr& searcher)
{
    const auto newEnd = std::remove_if(m_unfinishedSearches.begin(), m_unfinishedSearches.end(),
        [searcher](const SearcherPtr& currentSearcher)
        {
            return searcher == currentSearcher;
        });
    m_unfinishedSearches.erase(newEnd, m_unfinishedSearches.end());
}

void DeviceAdditionDialog::handleSearchTypeChanged()
{
    ui->searchButton->setText(ui->tabWidget->currentIndex()
        ? tr("Scan")
        : tr("Search"));
}

void DeviceAdditionDialog::handleDialogClosed()
{
    stopSearch();
    m_model.reset();
    updateResultsWidgetState();
}

void DeviceAdditionDialog::handleModelDataChanged(
    const QModelIndex& /*topLeft*/,
    const QModelIndex& /*bottomRight*/,
    const QVector<int>& roles)
{
    const bool checkedStateChanged = std::any_of(roles.begin(), roles.end(),
        [](int role) { return role == Qt::CheckStateRole; });

    if (checkedStateChanged)
        updateAddDevicesButtonText();
}

void DeviceAdditionDialog::updateAddDevicesButtonText()
{
    const auto checkedCount = ui->foundDevicesTable->getCheckedCount();
    ui->addDevicesButton->setText(checkedCount
        ? tr("Add %n Devices", "", checkedCount)
        : tr("Add all devices"));
}

QString DeviceAdditionDialog::progressMessage() const
{
    if (!m_currentSearch)
        return QString();

    switch(m_currentSearch->progress())
    {
        case QnManualResourceSearchStatus::Init:
            return lit("%1\t").arg(tr("Initializing scan"));
        case QnManualResourceSearchStatus::CheckingOnline:
            return lit("%1\t").arg(tr("Scanning online hosts"));
        case QnManualResourceSearchStatus::CheckingHost:
            return lit("%1\t").arg(tr("Checking host"));
        case QnManualResourceSearchStatus::Finished:
            return lit("%1\t").arg(tr("Finished"));
        case QnManualResourceSearchStatus::Aborted:
            return lit("%1\t").arg(tr("Aborted"));
        default:
            return QString();
    }
}

void DeviceAdditionDialog::updateResultsWidgetState()
{
    static constexpr int kResultsPageIndex = 0;
    static constexpr int kPlaceholderPageIndex = 1;
    const bool empty = !m_model || !m_model->rowCount();

    ui->searchResultsStackedWidget->setCurrentIndex(empty
        ? kPlaceholderPageIndex
        : kResultsPageIndex);

    if (!m_currentSearch)
    {
        const auto icon = qnSkin->icon("placeholders/adding_device_placeholder.png");
        ui->placeholderLabel->setPixmap(qnSkin->maximumSizePixmap(icon));
    }
    else
    {
        const auto progress = m_currentSearch->progress();
        const auto text = m_currentSearch->searching()
            ? tr("Searching...")
            : tr("No devices found");
        ui->placeholderLabel->setText(text.toUpper());
    }

    if (empty)
        setAccentStyle(ui->searchButton);
    else
        resetButtonStyle(ui->searchButton);


    const bool showSearchProgressControls = m_currentSearch && m_currentSearch->searching();
    ui->searchControls->setCurrentIndex(showSearchProgressControls ? 1 : 0);
    ui->searchButton->setVisible(!showSearchProgressControls);
    ui->stopSearchButton->setVisible(showSearchProgressControls);
    ui->searchProgressBar->setVisible(showSearchProgressControls);

    if (!m_currentSearch)
        return;

    ui->searchProgressBar->setFormat(progressMessage());

    if (m_currentSearch->progress() == QnManualResourceSearchStatus::Aborted
        && !m_currentSearch->lastErrorText().isEmpty())
    {
        QnMessageBox::critical(this, tr("Device search failed"));
    }
}
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

#include "device_addition_dialog.h"
#include "ui_device_addition_dialog.h"

#include "private/found_devices_model.h"
#include "private/manual_device_searcher.h"
#include "private/presented_state_delegate.h"

#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <nx/client/desktop/resource_views/views/fake_resource_list_view.h>
#include <nx/client/desktop/common/widgets/password_preview_button.h>
#include <nx/client/desktop/common/widgets/snapped_scroll_bar.h>

#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>
#include <utils/common/scoped_value_rollback.h>
#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

namespace {

using namespace nx::client::desktop;

QHostAddress fixedAddress(const QnIpLineEdit* edit)
{
    auto fixed = edit->text();
    edit->validator()->fixup(fixed);
    return QHostAddress(fixed);
}

bool isKnownAddressPage(QTabWidget* tabWidget)
{
    static constexpr auto kKnownAddressPageIndex = 0;
    return tabWidget->currentIndex() == kKnownAddressPageIndex;
}

FakeResourceList toFakeResourcesList(const QnManualResourceSearchList& devices)
{
    FakeResourceList result;

    for(const auto& device: devices)
    {
        const FakeResourceDescription camera =
            {
                device.uniqueId,
                qnSkin->icon("tree/camera.png"),
                device.name,
                device.url
            };

        result.append(camera);
    }

    return result;
}


using ResourceCallback = std::function<void (const QnResourcePtr& resource)>;
using CameraCallback = std::function<void (const QnVirtualCameraResourcePtr& camera)>;

ResourceCallback createCameraCallback(const CameraCallback& cameraCallback)
{
    return
        [cameraCallback](const QnResourcePtr& resource)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                cameraCallback(camera);
        };
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

DeviceAdditionDialog::DeviceAdditionDialog(QWidget* parent):
    base_type(parent),
    m_pool(commonModule()->resourcePool()),
    m_serversWatcher(parent),
    m_serverStatusWatcher(this),
    ui(new Ui::DeviceAdditionDialog())
{
    ui->setupUi(this);

    initializeControls();

    connect(m_pool, &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                setDeviceAdded(camera->getUniqueId());
        });
    connect(m_pool, &QnResourcePool::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
                handleDeviceRemoved(camera->getUniqueId());
        });
}

DeviceAdditionDialog::~DeviceAdditionDialog()
{
    stopSearch();
}

void DeviceAdditionDialog::initializeControls()
{
    SnappedScrollBar* scrollBar = new SnappedScrollBar(ui->tableHolder);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    scrollBar->setUseMaximumSpace(true);
    ui->foundDevicesTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    ui->foundDevicesTable->setSortingEnabled(false);

    ui->searchButton->setFocusPolicy(Qt::StrongFocus);
    ui->stopSearchButton->setFocusPolicy(Qt::StrongFocus);

    connect(ui->searchButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::handleStartSearchClicked);
    connect(ui->stopSearchButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::stopSearch);
    connect(ui->addDevicesButton, &QPushButton::clicked,
        this, &DeviceAdditionDialog::handleAddDevicesClicked);

    connect(ui->tabWidget, &QTabWidget::tabBarClicked,
        this, &DeviceAdditionDialog::handleTabClicked);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
        this, &DeviceAdditionDialog::handleSearchTypeChanged);

    const auto tabWidgetSizePolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    autoResizePagesToContents(ui->tabWidget, tabWidgetSizePolicy, true);
    setTabShape(ui->tabWidget->tabBar(), style::TabShape::Compact);

    connect(&m_serversWatcher, &CurrentSystemServers::serverAdded,
        ui->selectServerMenuButton, &QnChooseServerButton::addServer);
    connect(&m_serversWatcher, &CurrentSystemServers::serverRemoved,
        ui->selectServerMenuButton, &QnChooseServerButton::removeServer);
    for (const auto server: m_serversWatcher.servers())
        ui->selectServerMenuButton->addServer(server);

    ui->startAddressEdit->setPlaceholderText(tr("Start address"));
    ui->startAddressEdit->setText(lit("0.0.0.0"));
    ui->endAddressEdit->setPlaceholderText(tr("End address"));
    ui->endAddressEdit->setText(lit("0.0.0.255"));
    connect(ui->startAddressEdit, &QLineEdit::textChanged,
        this, &DeviceAdditionDialog::handleStartAddressFieldTextChanged);
    connect(ui->startAddressEdit, &QLineEdit::editingFinished,
        this, &DeviceAdditionDialog::handleStartAddressEditingFinished);
    connect(ui->endAddressEdit, &QLineEdit::textChanged,
        this, &DeviceAdditionDialog::handleEndAddressFieldTextChanged);

    ui->addressEdit->setPlaceholderText(tr("IP / Hostname / RTSP link / UDP link"));
    ui->addressEdit->setExternalControls(ui->addressLabel, ui->addressHint);
    ui->addressHint->setVisible(false);
    ui->explanationLabel->setPixmap(qnSkin->pixmap("buttons/context_info.png"));
    ui->explanationLabel->setToolTip(tr("Examples:")
        + lit("\n192.168.1.15\n"
              "www.example.com:8080\n"
              "http://example.com:7090/image.jpg\n"
              "rtsp://example.com:554/video\n"
              "udp://239.250.5.5:1234"));

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
    connect(&m_serversWatcher, &CurrentSystemServers::serversCountChanged,
        this, &DeviceAdditionDialog::updateSelectedServerButtonVisibility);
    connect(&m_serverStatusWatcher, &ServerOnlineStatusWatcher::statusChanged,
        this, &DeviceAdditionDialog::handleServerOnlineStateChanged);
    ui->serverOfflineAlertBar->setVisible(false);

    connect(this, &DeviceAdditionDialog::rejected,
        this, &DeviceAdditionDialog::handleDialogClosed);

    setupTable();
    setupPortStuff(ui->knownAddressAutoPortCheckBox, ui->knownAddressPortSpinWidget);
    setupPortStuff(ui->subnetScanAutoPortCheckBox, ui->subnetScanPortSpinWidget);

    setAccentStyle(ui->searchButton);
    setAccentStyle(ui->addDevicesButton);

    PasswordPreviewButton::createInline(ui->knownAddressPasswordEdit);
    PasswordPreviewButton::createInline(ui->subnetScanPasswordEdit);

    updateResultsWidgetState();

    handleTabClicked(ui->tabWidget->currentIndex());
}

void DeviceAdditionDialog::handleStartAddressFieldTextChanged(const QString& value)
{
    if (m_addressEditing)
        return;

    QnScopedValueRollback rollback(&m_addressEditing, true);
    ui->endAddressEdit->setText(fixedAddress(ui->startAddressEdit).toString());
}

void DeviceAdditionDialog::handleStartAddressEditingFinished()
{
    const auto startAddress = fixedAddress(ui->startAddressEdit).toIPv4Address();
    if (startAddress % 256 == 0)
        ui->endAddressEdit->setText(QHostAddress(startAddress + 255).toString());
}

void DeviceAdditionDialog::handleEndAddressFieldTextChanged(const QString& value)
{
    if (m_addressEditing)
        return;

    QnScopedValueRollback rollback(&m_addressEditing, true);

    const auto startAddress = fixedAddress(ui->startAddressEdit);
    const auto endAddress = fixedAddress(ui->endAddressEdit);

    const quint32 startSubnet = startAddress.toIPv4Address() >> 8;
    const quint32 endSubnet = endAddress.toIPv4Address() >> 8;
    if (startSubnet != endSubnet)
    {
        const QHostAddress fixed(startAddress.toIPv4Address() + ((endSubnet - startSubnet) << 8));
        ui->startAddressEdit->setText(fixed.toString());
    }
}

void DeviceAdditionDialog::handleTabClicked(int index)
{
    // We need two functions below to prevent blinking when page is changed.
    static const auto resetPageSize = [](QWidget* widget) { widget->setFixedHeight(0); };
    const auto setHeightFromLayout =
        [this](QWidget* widget)
        {
            const auto layout = widget->layout();
            widget->setFixedHeight(layout->minimumSize().height());
            executeLater(
                [widget, layout]() { widget->setMaximumHeight(layout->maximumSize().height()); },
                this);
        };

    if (index)
    {
        ui->addressEdit->setValidator(TextValidateFunction(), true);
        ui->startAddressEdit->setFocus();
        resetPageSize(ui->knownAddressesPage);
        setHeightFromLayout(ui->subnetScanPage);
    }
    else
    {
        ui->addressEdit->setValidator(
            defaultNonEmptyValidator(tr("Address field can't be empty")));
        ui->addressEdit->setFocus();
        resetPageSize(ui->subnetScanPage);
        setHeightFromLayout(ui->knownAddressesPage);
    }
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

    table->setCheckboxColumn(FoundDevicesModel::checkboxColumn);
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

void DeviceAdditionDialog::setupPortStuff(
    QCheckBox* autoCheckbox,
    QStackedWidget* portStackedWidget)
{
    const auto handleAutoChecked =
        [portStackedWidget](bool checked)
        {
            static constexpr int kSpinBoxPage = 0;
            static constexpr int kPlaceholderPage = 1;
            portStackedWidget->setCurrentIndex(checked
                ? kPlaceholderPage
                : kSpinBoxPage);
        };

    connect(autoCheckbox, &QCheckBox::toggled, this, handleAutoChecked);
    handleAutoChecked(autoCheckbox->isChecked());
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

    if (isKnownAddressPage(ui->tabWidget))
    {
        if (!ui->addressEdit->isValid())
        {
            ui->addressEdit->setFocus();
            ui->addressEdit->validate();
            return;
        }

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

        QnMessageBox::critical(this, tr("Device search failed"), m_currentSearch->initialError());
        stopSearch();
        return;
    }

    resetButtonStyle(ui->searchButton);

    m_model.reset(new FoundDevicesModel(ui->foundDevicesTable));

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

    ui->stopSearchButton->setFocus();
}

void DeviceAdditionDialog::appendAddingDevices(const AddingDevicesSet& value)
{
    m_addingDevices += value;
}

void DeviceAdditionDialog::setDeviceAdded(const QString& uniqueId)
{
    if (!m_addingDevices.remove(uniqueId))
        return;

    const auto index = m_model->indexByUniqueId(uniqueId, FoundDevicesModel::presentedStateColumn);
    if (!index.isValid())
        return;

    m_model->setData(
        index, FoundDevicesModel::alreadyAddedState, FoundDevicesModel::presentedStateRole);
}

void DeviceAdditionDialog::handleDeviceRemoved(const QString& uniqueId)
{
    const auto index = m_model->indexByUniqueId(uniqueId, FoundDevicesModel::presentedStateColumn);
    if (!index.isValid())
        return;

    const auto state = index.data(FoundDevicesModel::presentedStateRole);
    if (state == FoundDevicesModel::alreadyAddedState)
        m_model->setData(index, FoundDevicesModel::notPresentedState, FoundDevicesModel::presentedStateRole);
    else
        NX_ASSERT(false, "Wrong presented state");
}

void DeviceAdditionDialog::handleAddDevicesClicked()
{
    const auto server = ui->selectServerMenuButton->currentServer();
    if (!server || server->getStatus() != Qn::Online || !m_currentSearch || !m_model)
        return;

    QnManualResourceSearchList devices;

    AddingDevicesSet addingDevices;

    const bool checkSelection = ui->foundDevicesTable->getCheckedCount();
    const int rowsCount = m_model->rowCount();
    for (int row = 0; row != rowsCount; ++row)
    {
        const auto choosenIndex = m_model->index(row, FoundDevicesModel::checkboxColumn);
        if (checkSelection && !m_model->data(choosenIndex, Qt::CheckStateRole).toBool())
            continue;

        const auto stateIndex = m_model->index(row, FoundDevicesModel::presentedStateColumn);
        const auto presentedState = stateIndex.data(FoundDevicesModel::presentedStateRole);
        if (presentedState == FoundDevicesModel::alreadyAddedState)
            continue;

        const auto dataIndex = m_model->index(row);
        const auto device = m_model->data(dataIndex, FoundDevicesModel::dataRole)
            .value<QnManualResourceSearchEntry>();

        m_model->setData(stateIndex, FoundDevicesModel::addingInProgressState,
            FoundDevicesModel::presentedStateRole);

        devices.append(device);
        addingDevices.insert(device.uniqueId);
    }

    if (devices.isEmpty())
        return;

    const auto login = m_currentSearch->login();
    const auto password = m_currentSearch->password();
    server->restConnection()->addCamera(devices, login, password,
        [this, addingDevices, devices, guard = QPointer<DeviceAdditionDialog>(this)]
            (bool success, rest::Handle /*handle*/, const QnJsonRestResult& result)
        {
            if (guard && success)
                appendAddingDevices(addingDevices);

//            auto states = prevStates;
//            if (success && result.error == QnRestResult::NoError)
//            {
//                for (auto& state: states)
//                    state = FoundDevicesModel::alreadyAddedState;
//            }
//            else
//            {
//                showAdditionFailedDialog(toFakeResourcesList(devices));
//            }

//            for (auto it = states.begin(); it != states.end(); ++it)
//            {
//                const auto id = it.key();
//                const auto state = it.value();
//                const auto index = m_model->indexByUniqueId(
//                    id, FoundDevicesModel::presentedStateColumn);
//                m_model->setData(index, state, FoundDevicesModel::presentedStateRole);
//            }
        }, QThread::currentThread());
}

void DeviceAdditionDialog::showAdditionFailedDialog(const FakeResourceList& resources)
{
    QnMessageBox dialog(QnMessageBoxIcon::Critical,
        tr("Failed to add %n devices", "", resources.size()), QString(),
        QDialogButtonBox::Ok, QDialogButtonBox::Ok,
        this);

    dialog.addCustomWidget(new FakeResourceListView(resources, this));
    dialog.exec();
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
        && !m_currentSearch->initialError().isEmpty())
    {
        QnMessageBox::critical(this, tr("Device search failed"));
    }
}

} // namespace desktop
} // namespace client
} // namespace nx

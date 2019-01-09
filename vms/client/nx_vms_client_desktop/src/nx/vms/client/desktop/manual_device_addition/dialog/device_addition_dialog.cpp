#include "device_addition_dialog.h"
#include "ui_device_addition_dialog.h"
#include "private/found_devices_model.h"
#include "private/manual_device_searcher.h"
#include "private/presented_state_delegate.h"

#include <QtCore/QScopedValueRollback>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <utils/common/event_processors.h>
#include <utils/common/delayed.h>

#include <nx/vms/client/desktop/resource_views/views/fake_resource_list_view.h>
#include <nx/vms/client/desktop/common/widgets/password_preview_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/utils/guarded_callback.h>

namespace {

using namespace nx::vms::client::desktop;

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

namespace nx::vms::client::desktop {

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

    const auto stackedWidgetSizePolicy = QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    autoResizePagesToContents(ui->stackedWidget, stackedWidgetSizePolicy, true);
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
    ui->addressEdit->setHintColor(QPalette().color(QPalette::WindowText));
    ui->addressHint->setVisible(false);
    ui->explanationLabel->setPixmap(qnSkin->pixmap("buttons/context_info.png"));
    ui->explanationLabel->setToolTip(tr("Examples:")
        + lit("\n192.168.1.15\n"
              "www.example.com:8080\n"
              "http://example.com:7090/image.jpg\n"
              "rtsp://example.com:554/video\n"
              "udp://239.250.5.5:1234"));

    ui->widget->setFixedWidth(ui->addressLabelLayout->minimumSize().width());
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

    connect(ui->searchControls, &QStackedWidget::currentChanged, this,
        [this](int currentPageIndex)
        {
            static constexpr int kStartSearchPage = 0;
            const bool enabled = currentPageIndex == kStartSearchPage;
            const QList<QWidget*> widgets =
                {ui->searchParametersPanel, ui->addDevicesButtonPage, ui->serverChoosePanel};
            for (const auto widget: widgets)
                widget->setEnabled(enabled);
            for (int i = 0; i < ui->tabWidget->count(); ++i)
            {
                if (i != ui->tabWidget->currentIndex())
                    ui->tabWidget->setTabEnabled(i, enabled);
            }
        });

    setupTable();
    setupPortStuff(ui->knownAddressAutoPortCheckBox, ui->knownAddressPortSpinWidget);
    setupPortStuff(ui->subnetScanAutoPortCheckBox, ui->subnetScanPortSpinWidget);

    setAccentStyle(ui->searchButton);
    setAccentStyle(ui->addDevicesButton);

    PasswordPreviewButton::createInline(ui->passwordEdit);

    updateResultsWidgetState();

    connect(ui->tabWidget, &QTabWidget::currentChanged,
        ui->stackedWidget, &QStackedWidget::setCurrentIndex);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
        this, &DeviceAdditionDialog::handleSearchTypeChanged);
    connect(ui->tabWidget, &QTabWidget::tabBarClicked,
        this, &DeviceAdditionDialog::handleTabClicked);

    ui->tabWidget->setCurrentIndex(0);
    handleTabClicked(ui->tabWidget->currentIndex());
    setupSearchResultsPlaceholder();
}

void DeviceAdditionDialog::handleStartAddressFieldTextChanged(const QString& value)
{
    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);
    ui->endAddressEdit->setText(fixedAddress(ui->startAddressEdit).toString());
}

void DeviceAdditionDialog::handleStartAddressEditingFinished()
{
    const auto startAddress = fixedAddress(ui->startAddressEdit).toIPv4Address();
    const auto endAddress = fixedAddress(ui->endAddressEdit).toIPv4Address();
    if (!ui->endAddressEdit->isModified() && (startAddress / 256) != (endAddress / 256))
    {
        ui->endAddressEdit->setText(
            QHostAddress(startAddress - startAddress % 256 + 255).toString());
    }
}

void DeviceAdditionDialog::handleEndAddressFieldTextChanged(const QString& value)
{
    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);

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
        resetPageSize(ui->knownAddressPage);
        setHeightFromLayout(ui->subnetScanPage);
        m_knownAddressCredentials.user = ui->loginEdit->text().trimmed();
        m_knownAddressCredentials.password = ui->passwordEdit->text().trimmed();
        ui->loginEdit->setText(m_subnetScanCredentials.user);
        ui->passwordEdit->setText(m_subnetScanCredentials.password);
    }
    else
    {
        ui->addressEdit->setValidator(
            defaultNonEmptyValidator(tr("Address field can't be empty")));
        ui->addressEdit->setFocus();
        resetPageSize(ui->subnetScanPage);
        setHeightFromLayout(ui->knownAddressPage);
        m_subnetScanCredentials.user = ui->loginEdit->text().trimmed();
        m_subnetScanCredentials.password = ui->passwordEdit->text().trimmed();
        ui->loginEdit->setText(m_knownAddressCredentials.user);
        ui->passwordEdit->setText(m_knownAddressCredentials.password);
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

void DeviceAdditionDialog::setupSearchResultsPlaceholder()
{
    QFont font;
    font.setPixelSize(24);
    font.setWeight(QFont::Light);
    ui->placeholderLabel->setFont(font);
    setPaletteColor(ui->placeholderLabel, QPalette::All, QPalette::WindowText,
        QPalette().color(QPalette::Midlight));
}

void DeviceAdditionDialog::setServer(const QnMediaServerResourcePtr& value)
{
    if (!ui->selectServerMenuButton->setCurrentServer(value))
        stopSearch();
}

std::optional<int> DeviceAdditionDialog::port() const
{
    const bool isKnownAddressTabPage = isKnownAddressPage(ui->tabWidget);
    const auto portSpinBox = isKnownAddressTabPage
        ? ui->knownAddressPortSpinBox
        : ui->subnetScanPortSpinBox;
    const auto autoPortSpinBox = isKnownAddressTabPage
        ? ui->knownAddressAutoPortCheckBox
        : ui->subnetScanAutoPortCheckBox;

    if (autoPortSpinBox->isChecked())
        return std::nullopt;
    return portSpinBox->value();
}

QString DeviceAdditionDialog::password() const
{
    return ui->passwordEdit->text().trimmed();
}

QString DeviceAdditionDialog::login() const
{
    return ui->loginEdit->text().trimmed();
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
        m_lastSearchLogin = login();
        m_lastSearchPassword = password();
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->addressEdit->text().simplified(), login(), password(), port()));
    }
    else
    {
        m_lastSearchLogin = login();
        m_lastSearchPassword = password();
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->startAddressEdit->text().trimmed(), ui->endAddressEdit->text().trimmed(),
            login(), password(), port()));
    }

    if (m_currentSearch->status().state == QnManualResourceSearchStatus::Aborted)
    {

        QnMessageBox::critical(this, tr("Device search failed"), m_currentSearch->initialError());
        stopSearch();
        return;
    }

    resetButtonStyle(ui->searchButton);

    m_model.reset(new FoundDevicesModel(ui->foundDevicesTable));

    updateResultsWidgetState();

    connect(m_currentSearch, &ManualDeviceSearcher::statusChanged,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model, &FoundDevicesModel::rowsInserted,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model, &FoundDevicesModel::rowsRemoved,
        this, &DeviceAdditionDialog::updateResultsWidgetState);

    connect(m_model, &FoundDevicesModel::dataChanged,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model, &FoundDevicesModel::rowsInserted,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model, &FoundDevicesModel::rowsRemoved,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);

    ui->foundDevicesTable->setModel(m_model.data());
    setupTableHeader();

    connect(m_currentSearch, &ManualDeviceSearcher::devicesAdded,
        m_model, &FoundDevicesModel::addDevices);
    connect(m_currentSearch, &ManualDeviceSearcher::devicesRemoved,
        m_model, &FoundDevicesModel::removeDevices);

    ui->stopSearchButton->setFocus();
}

void DeviceAdditionDialog::setDeviceAdded(const QString& uniqueId)
{
    if (!m_model)
        return;
    const auto index = m_model->indexByUniqueId(uniqueId, FoundDevicesModel::presentedStateColumn);
    if (!index.isValid())
        return;

    m_model->setData(
        index, FoundDevicesModel::alreadyAddedState, FoundDevicesModel::presentedStateRole);

    m_model->setData(index.siblingAtColumn(FoundDevicesModel::checkboxColumn),
        Qt::Unchecked, Qt::CheckStateRole);
}

void DeviceAdditionDialog::handleDeviceRemoved(const QString& uniqueId)
{
    if (!m_model)
        return;

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
    if (!server || server->getStatus() != Qn::Online || !m_model)
        return;

    QnManualResourceSearchList devices;

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

        m_model->setData(stateIndex.siblingAtColumn(FoundDevicesModel::checkboxColumn),
            Qt::Unchecked, Qt::CheckStateRole);

        devices.append(device);
    }

    if (devices.isEmpty())
        return;

    server->restConnection()->addCamera(devices, m_lastSearchLogin, m_lastSearchPassword, {});
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
        connect(m_currentSearch, &ManualDeviceSearcher::statusChanged, this,
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

QString DeviceAdditionDialog::progressMessage() const
{
    if (!m_currentSearch)
        return QString();

    switch(m_currentSearch->status().state)
    {
        case QnManualResourceSearchStatus::Init:
            return lit("%1\t").arg(tr("Initializing scan..."));
        case QnManualResourceSearchStatus::CheckingOnline:
            return lit("%1\t").arg(tr("Scanning online hosts..."));
        case QnManualResourceSearchStatus::CheckingHost:
            return lit("%1\t").arg(tr("Checking host..."));
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
        const auto status = m_currentSearch->status();
        const int total = status.total ? status.total : 1;
        const int current = status.total ? status.current : 0;
        ui->searchProgressBar->setMaximum(total);
        ui->searchProgressBar->setValue(current);
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

    if (m_currentSearch->status().state == QnManualResourceSearchStatus::Aborted
        && !m_currentSearch->initialError().isEmpty())
    {
        QnMessageBox::critical(this, tr("Device search failed"));
    }
}

void DeviceAdditionDialog::updateAddDevicesPanel()
{
    if (!m_model || m_model->rowCount() == 0)
        return;

    const int devicesCount = m_model->rowCount();
    const int newDevicesCheckedCount = m_model->deviceCount(FoundDevicesModel::notPresentedState,
        /*isChecked*/ true);
    const int newDevicesCount = m_model->deviceCount(FoundDevicesModel::notPresentedState);
    const int addingDevicesCount = m_model->deviceCount(FoundDevicesModel::addingInProgressState);
    const int addedDevicesCount = m_model->deviceCount(FoundDevicesModel::alreadyAddedState);

    if (devicesCount == addedDevicesCount)
    {
        showAddDevicesPlaceholder(tr("All devices are already added"));
    }
    else if (addingDevicesCount != 0 && newDevicesCheckedCount == 0)
    {
        showAddDevicesPlaceholder(
            tr("%n devices are being added. You can close this dialog or start a new search",
                nullptr, addingDevicesCount));
    }
    else if (newDevicesCount != 0
        && (newDevicesCheckedCount == 0 || newDevicesCount == newDevicesCheckedCount))
    {
        showAddDevicesButton(tr("Add all Devices"));
    }
    else if (newDevicesCount != 0 && newDevicesCheckedCount != 0)
    {
        showAddDevicesButton(tr("Add %n Devices", nullptr, newDevicesCheckedCount));
    }
}

void DeviceAdditionDialog::showAddDevicesButton(const QString& buttonText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesButtonPage);
    ui->addDevicesButton->setText(buttonText);
}

void DeviceAdditionDialog::showAddDevicesPlaceholder(const QString& placeholderText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesPlaceholderPage);
    ui->addDevicesPlaceholder->setText(placeholderText);
}

} // namespace nx::vms::client::desktop

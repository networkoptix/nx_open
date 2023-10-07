// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_addition_dialog.h"
#include "ui_device_addition_dialog.h"

#include <QtCore/QScopedValueRollback>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/validators.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/resource_views/views/fake_resource_list_view.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include "private/found_devices_delegate.h"
#include "private/found_devices_model.h"
#include "private/manual_device_searcher.h"
#include "private/presented_state_delegate.h"

namespace nx::vms::client::desktop {

namespace {

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

} // namespace

SortModel::SortModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    m_predicate.setNumericMode(true);
}

bool SortModel::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    return m_predicate(source_left.data().toString(), source_right.data().toString());
}

DeviceAdditionDialog::DeviceAdditionDialog(QWidget* parent):
    base_type(parent),
    m_serversWatcher(parent),
    m_serverStatusWatcher(this),
    ui(new Ui::DeviceAdditionDialog()),
    m_sortModel(new SortModel(this))
{
    ui->setupUi(this);

    initializeControls();

    auto urlChangeListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(this);

    urlChangeListener->setOnAddedHandler(
        [this](const QnVirtualCameraResourceList& cameras)
        {
            for (const auto& camera: cameras)
                setDeviceAdded(camera->getPhysicalId());
        });

    urlChangeListener->addOnSignalHandler(
        &QnResource::urlChanged,
        [this](const QnVirtualCameraResourcePtr& camera)
        {
            setDeviceAdded(camera->getPhysicalId());
        });

    urlChangeListener->setOnRemovedHandler(
        [this](const QnVirtualCameraResourceList& cameras)
        {
            for (const auto& camera: cameras)
                handleDeviceRemoved(camera->getPhysicalId());
        });

    urlChangeListener->start();
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

    ui->foundDevicesTable->setSortingEnabled(true);

    ui->searchButton->setFocusPolicy(Qt::StrongFocus);
    ui->stopSearchButton->setFocusPolicy(Qt::StrongFocus);
    ui->addDevicesButton->setFocusPolicy(Qt::StrongFocus);
    ui->knownAddressAutoPortCheckBox->setFocusPolicy(Qt::StrongFocus);
    ui->foundDevicesTable->setFocusPolicy(Qt::StrongFocus);

    ui->foundDevicesTable->setModel(m_sortModel);

    // Monitor focus and change accent button accodringly.
    QWidget* const filteredWidgets[] = {
        ui->searchButton,
        ui->startAddressEdit,
        ui->endAddressEdit,
        ui->addressEdit->lineEdit(),
        ui->knownAddressAutoPortCheckBox,
        ui->knownAddressPortSpinBox,
        ui->subnetScanAutoPortCheckBox,
        ui->subnetScanPortSpinBox,
        ui->loginEdit,
        ui->passwordEdit,
        ui->stopSearchButton,
        ui->foundDevicesTable,
        ui->addDevicesButton
    };
    for (auto widget: filteredWidgets)
        widget->installEventFilter(this);

    ui->searchButton->setAutoDefault(true);
    ui->stopSearchButton->setAutoDefault(true);
    ui->addDevicesButton->setAutoDefault(false);

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
    for (const auto& server: m_serversWatcher.servers())
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
    ui->addressEdit->setUseWarningStyleForControl(false);
    ui->addressHint->setVisible(false);
    ui->addressLabel->addHintLine(tr("Examples:"));
    ui->addressLabel->addHintLine(lit("192.168.1.15"));
    ui->addressLabel->addHintLine(lit("www.example.com:8080"));
    ui->addressLabel->addHintLine(lit("http://example.com:7090/image.jpg"));
    ui->addressLabel->addHintLine(lit("rtsp://example.com:554/video"));
    ui->addressLabel->addHintLine(lit("udp://239.250.5.5:1234"));

    ui->widget->setFixedWidth(ui->addressLabelLayout->minimumSize().width());
    installEventHandler(ui->serverChoosePanel, QEvent::PaletteChange, ui->serverChoosePanel,
        [this]()
        {
            setPaletteColor(ui->serverChoosePanel, QPalette::Window,
                palette().color(QPalette::Mid));
        });

    connect(ui->selectServerMenuButton, &QnChooseServerButton::currentServerChanged,
        this, &DeviceAdditionDialog::handleSelectedServerChanged);

    updateSelectedServerButtonVisibility();
    connect(&m_serversWatcher, &CurrentSystemServers::serversCountChanged,
        this, &DeviceAdditionDialog::updateSelectedServerButtonVisibility);
    connect(&m_serverStatusWatcher, &ServerOnlineStatusWatcher::statusChanged,
        this, &DeviceAdditionDialog::handleServerOnlineStateChanged);

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

    setAddDevicesAccent(false);

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

    setPaletteColor(ui->knownAddressPortPlaceholder, QPalette::Text, core::colorTheme()->color("dark14"));
    setPaletteColor(ui->subnetScanPortPlaceholder, QPalette::Text, core::colorTheme()->color("dark14"));

    ui->serverOfflineAlertBar->init({.text=tr("Server offline"),
        .level = BarDescription::BarLevel::Error});
    ui->serverOfflineAlertBar->setVisible(false);
    ui->httpsOnlyBar->init(
        {.text = tr("Searching for devices on the network is restricted to cameras that"
                    " support HTTPS connections. This can be changed in %1 settings.")
                     .arg(nx::vms::common::html::localLink(tr("System Administration"))),
            .level = BarDescription::BarLevel::Info,
            .isOpenExternalLinks = false,
            .isEnabledProperty = &messageBarSettings()->httpsOnlyBarInfo});
    ui->httpsOnlyBar->setVisible(false);
    connect(ui->httpsOnlyBar,
        &CommonMessageBar::linkActivated,
        [this]()
        {
            menu()->trigger(ui::action::SystemAdministrationAction,
                ui::action::Parameters().withArgument(
                    Qn::FocusTabRole, QnSystemAdministrationDialog::SecurityPage));
        });
}

void DeviceAdditionDialog::setAddDevicesAccent(bool canAddDevices)
{
    const auto accentButton = canAddDevices ? ui->addDevicesButton : ui->searchButton;
    const auto regularButton = accentButton == ui->addDevicesButton ? ui->searchButton : ui->addDevicesButton;

    regularButton->setDefault(false);
    resetButtonStyle(regularButton);

    accentButton->setDefault(true);
    setAccentStyle(accentButton);
}

bool DeviceAdditionDialog::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        const bool addDevicesFocus = object == ui->foundDevicesTable || object == ui->addDevicesButton;
        const bool newDevicesPresent = m_model && m_model->deviceCount(FoundDevicesModel::notPresentedState) > 0;
        setAddDevicesAccent(addDevicesFocus && newDevicesPresent);
    }
    return base_type::eventFilter(object, event);
}

void DeviceAdditionDialog::handleStartAddressFieldTextChanged(const QString& /*value*/)
{
    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);

    const auto startAddress = fixedAddress(ui->startAddressEdit).toIPv4Address();
    const auto endAddress = fixedAddress(ui->endAddressEdit).toIPv4Address();

    ui->endAddressEdit->setText(
        QHostAddress(startAddress - startAddress % 256 + endAddress % 256).toString());
}

void DeviceAdditionDialog::handleStartAddressEditingFinished()
{
    const auto startAddress = fixedAddress(ui->startAddressEdit).toIPv4Address();
    const auto endAddress = fixedAddress(ui->endAddressEdit).toIPv4Address();

    if (startAddress % 256 > endAddress % 256)
    {
        ui->endAddressEdit->setText(
            QHostAddress(startAddress - startAddress % 256 + 255).toString());
    }
}

void DeviceAdditionDialog::handleEndAddressFieldTextChanged(const QString& /*value*/)
{
    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);

    const auto startAddress = fixedAddress(ui->startAddressEdit).toIPv4Address();
    const auto endAddress = fixedAddress(ui->endAddressEdit).toIPv4Address();

    ui->startAddressEdit->setText(
        QHostAddress(endAddress - endAddress % 256 + startAddress % 256).toString());
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
            defaultNonEmptyValidator(tr("Address field cannot be empty")));
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
    table->setItemDelegate(new FoundDevicesDelegate(table));
    table->setPersistentDelegateForColumn(FoundDevicesModel::presentedStateColumn,
        new PresentedStateDelegate(table));
}

void DeviceAdditionDialog::setupTableHeader()
{
    const auto header = ui->foundDevicesTable->horizontalHeader();

    header->resizeSection(FoundDevicesModel::brandColumn, 140);
    header->resizeSection(FoundDevicesModel::modelColumn, 140);

    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(
        FoundDevicesModel::addressColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        FoundDevicesModel::checkboxColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        FoundDevicesModel::presentedStateColumn, QHeaderView::ResizeToContents);

    ui->foundDevicesTable->horizontalHeader()->setSectionsClickable(true);
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
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->addressEdit->text().simplified(),
            login(),
            password(),
            port()));
    }
    else
    {
        m_currentSearch.reset(new ManualDeviceSearcher(ui->selectServerMenuButton->currentServer(),
            ui->startAddressEdit->text().trimmed(),
            ui->endAddressEdit->text().trimmed(),
            login(),
            password(),
            port()));
    }

    if (m_currentSearch->status().state == QnManualResourceSearchStatus::Aborted)
    {
        // TODO: #spanasenko Use static helpers here when they'll be implemented.
        QnSessionAwareMessageBox messageBox(this);
        messageBox.setText(tr("Device search failed"));
        messageBox.setInformativeText(m_currentSearch->initialError());
        messageBox.setIcon(QnMessageBoxIcon::Critical);
        messageBox.addButton(QDialogButtonBox::Ok);
        messageBox.setDefaultButton(QDialogButtonBox::Ok);
        messageBox.exec();

        stopSearch();
        return;
    }

    resetButtonStyle(ui->searchButton);

    m_model.reset(new FoundDevicesModel(ui->foundDevicesTable));
    m_sortModel->setSourceModel(m_model.data());

    updateResultsWidgetState();

    connect(m_currentSearch.data(), &ManualDeviceSearcher::statusChanged,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model.data(), &FoundDevicesModel::rowsInserted,
        this, &DeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model.data(), &FoundDevicesModel::rowsRemoved,
        this, &DeviceAdditionDialog::updateResultsWidgetState);

    connect(m_model.data(), &FoundDevicesModel::dataChanged,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model.data(), &FoundDevicesModel::rowsInserted,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model.data(), &FoundDevicesModel::rowsRemoved,
        this, &DeviceAdditionDialog::updateAddDevicesPanel);

    setupTableHeader();

    connect(m_currentSearch.data(), &ManualDeviceSearcher::devicesAdded,
        m_model.data(), &FoundDevicesModel::addDevices);
    connect(m_currentSearch.data(), &ManualDeviceSearcher::devicesRemoved,
        m_model.data(), &FoundDevicesModel::removeDevices);

    ui->stopSearchButton->setFocus();
}

void DeviceAdditionDialog::setDeviceAdded(const QString& physicalId)
{
    if (!m_model)
        return;

    const auto index = m_model->indexByPhysicalId(physicalId, FoundDevicesModel::presentedStateColumn);
    if (!index.isValid())
        return;

    m_model->setData(
        index, FoundDevicesModel::alreadyAddedState, FoundDevicesModel::presentedStateRole);

    m_model->setData(index.siblingAtColumn(FoundDevicesModel::checkboxColumn),
        Qt::Unchecked, Qt::CheckStateRole);
}

void DeviceAdditionDialog::handleDeviceRemoved(const QString& physicalId)
{
    if (!m_model)
        return;

    const auto index = m_model->indexByPhysicalId(physicalId, FoundDevicesModel::presentedStateColumn);
    if (!index.isValid())
        return;

    const auto state = index.data(FoundDevicesModel::presentedStateRole);
    if (NX_ASSERT(state == FoundDevicesModel::alreadyAddedState,
        "Wrong presented state: %1", state))
    {
        m_model->setData(
            index,
            FoundDevicesModel::notPresentedState,
            FoundDevicesModel::presentedStateRole);
    };
}

void DeviceAdditionDialog::handleAddDevicesClicked()
{
    const auto server = ui->selectServerMenuButton->currentServer();
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online || !m_model)
        return;

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
            .value<api::DeviceModelForSearch>();

        m_model->setData(stateIndex, FoundDevicesModel::addingInProgressState,
            FoundDevicesModel::presentedStateRole);

        m_model->setData(stateIndex.siblingAtColumn(FoundDevicesModel::checkboxColumn),
            Qt::Unchecked, Qt::CheckStateRole);

        connectedServerApi()->addCamera(
            server->getId(),
            device,
            systemContext()->getSessionTokenHelper(),
            /*callback*/ {});
    }
}

void DeviceAdditionDialog::showAdditionFailedDialog(const FakeResourceList& resources)
{
    // TODO: #spanasenko Use static helpers here when they'll be implemented.
    QnSessionAwareMessageBox messageBox(this);
    messageBox.setText(tr("Failed to add %n devices", "", resources.size()));
    messageBox.setIcon(QnMessageBoxIcon::Critical);
    messageBox.addButton(QDialogButtonBox::Ok);
    messageBox.setDefaultButton(QDialogButtonBox::Ok);
    messageBox.addCustomWidget(new FakeResourceListView(resources, this));
    messageBox.exec();
}

void DeviceAdditionDialog::stopSearch()
{
    if (!m_currentSearch)
        return;

    m_currentSearch->disconnect(this);
    m_currentSearch->disconnect(m_model.data());

    if (m_currentSearch->searching())
    {
        m_currentSearch->stop();
        m_unfinishedSearches.append(m_currentSearch);

        // Next state may be only "Finished".
        connect(m_currentSearch.data(), &ManualDeviceSearcher::statusChanged, this,
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
        const auto pixmap = qnSkin->pixmap(
            "placeholders/adding_devices_placeholder.svg", true, QSize(128, 128));
        ui->placeholderLabel->setPixmap(qnSkin->maximumSizePixmap(pixmap));
    }
    else
    {
        const auto status = m_currentSearch->status();
        const auto total = status.total ? status.total : 1;
        const auto current = status.total ? status.current : 0;
        ui->searchProgressBar->setMaximum(static_cast<int>(total));
        ui->searchProgressBar->setValue(static_cast<int>(current));
        const bool isSearching = m_currentSearch->searching();
        const auto text = isSearching
            ? tr("Searching...")
            : tr("No devices found");
        setAddDevicesAccent(isSearching);
        ui->placeholderLabel->setText(text.toUpper());
    }

    const bool showSearchProgressControls = m_currentSearch && m_currentSearch->searching();
    ui->searchControls->setCurrentIndex(showSearchProgressControls ? 1 : 0);
    ui->searchButton->setVisible(!showSearchProgressControls);
    ui->stopSearchButton->setVisible(showSearchProgressControls);
    ui->searchProgressBar->setVisible(showSearchProgressControls);
    ui->httpsOnlyBar->setVisible(m_currentSearch && systemSettings()->useHttpsOnlyForCameras()
        && messageBarSettings()->httpsOnlyBarInfo.value());

    if (!m_currentSearch)
        return;

    ui->searchProgressBar->setFormat(progressMessage());

    if (m_currentSearch->status().state == QnManualResourceSearchStatus::Aborted
        && !m_currentSearch->initialError().isEmpty())
    {
        // TODO: #spanasenko Use static helpers here when they'll be implemented.
        QnSessionAwareMessageBox messageBox(this);
        messageBox.setText(tr("Device search failed"));
        messageBox.setInformativeText(m_currentSearch->initialError());
        messageBox.setIcon(QnMessageBoxIcon::Critical);
        messageBox.addButton(QDialogButtonBox::Ok);
        messageBox.setDefaultButton(QDialogButtonBox::Ok);
        messageBox.exec();
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
    else if (newDevicesCount != 0)
    {
        if (newDevicesCheckedCount == devicesCount
            || (newDevicesCheckedCount == 0 && newDevicesCount == devicesCount))
        {
            showAddDevicesButton(tr("Add all Devices"));
        }
        else
        {
            showAddDevicesButton(tr("Add %n Devices", nullptr,
                newDevicesCheckedCount != 0 ? newDevicesCheckedCount : newDevicesCount));
        }
    }
}

void DeviceAdditionDialog::showAddDevicesButton(const QString& buttonText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesButtonPage);
    ui->addDevicesButton->setText(buttonText);
    ui->foundDevicesTable->setFocus();
}

void DeviceAdditionDialog::showAddDevicesPlaceholder(const QString& placeholderText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesPlaceholderPage);
    ui->addDevicesPlaceholder->setText(placeholderText);
    setAddDevicesAccent(false);
}

} // namespace nx::vms::client::desktop

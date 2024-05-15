// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "new_device_addition_dialog.h"
#include "ui_new_device_addition_dialog.h"

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
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/resource_views/views/fake_resource_list_view.h>
#include <nx/vms/client/desktop/settings/message_bar_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/palette.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/system_administration_dialog.h>
#include <ui/widgets/common/ip_line_edit.h>
#include <utils/common/delayed.h>
#include <utils/common/event_processors.h>

#include "private/add_device_address_parser.h"
#include "private/found_devices_delegate.h"
#include "private/found_devices_model.h"
#include "private/manual_device_searcher.h"
#include "private/presented_state_delegate.h"

namespace nx::vms::client::desktop {

NewSortModel::NewSortModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    m_predicate.setNumericMode(true);
}

bool NewSortModel::lessThan(const QModelIndex& source_left, const QModelIndex& source_right) const
{
    return m_predicate(source_left.data().toString(), source_right.data().toString());
}

NewDeviceAdditionDialog::NewDeviceAdditionDialog(QWidget* parent):
    base_type(parent),
    m_serversWatcher(systemContext()),
    m_serverStatusWatcher(),
    ui(new Ui::NewDeviceAdditionDialog()),
    m_sortModel(new NewSortModel(this))
{
    ui->setupUi(this);

    initializeControls();

    auto urlChangeListener =
        new core::SessionResourcesSignalListener<QnVirtualCameraResource>(system(), this);

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

NewDeviceAdditionDialog::~NewDeviceAdditionDialog()
{
    stopSearch();
}

void NewDeviceAdditionDialog::initializeControls()
{
    SnappedScrollBar* scrollBar = new SnappedScrollBar(ui->tableHolder);
    scrollBar->setUseItemViewPaddingWhenVisible(true);
    scrollBar->setUseMaximumSpace(true);
    ui->foundDevicesTable->setVerticalScrollBar(scrollBar->proxyScrollBar());

    ui->foundDevicesTable->setSortingEnabled(true);

    ui->searchButton->setFocusPolicy(Qt::StrongFocus);
    ui->foundDevicesTable->setFocusPolicy(Qt::StrongFocus);

    ui->foundDevicesTable->setModel(m_sortModel);

    // Monitor focus and change accent button accordingly.
    QWidget* const filteredWidgets[] = {
        ui->searchButton,
        ui->addressOrSubnetMaskEdit,
        ui->addressOrSubnetMaskEdit->lineEdit(),
        ui->portSpinBox,
        ui->loginEdit,
        ui->passwordEdit,
        ui->foundDevicesTable,
        ui->addDevicesButton
    };
    for (auto widget: filteredWidgets)
        widget->installEventFilter(this);

    ui->searchButton->setAutoDefault(true);
    ui->addDevicesButton->setAutoDefault(false);

    connect(ui->addDevicesButton, &QPushButton::clicked,
        this,
        &NewDeviceAdditionDialog::handleAddDevicesClicked);

    connect(&m_serversWatcher, &CurrentSystemServers::serverAdded,
        ui->selectServerMenuButton, &QnChooseServerButton::addServer);
    connect(&m_serversWatcher, &CurrentSystemServers::serverRemoved,
        ui->selectServerMenuButton, &QnChooseServerButton::removeServer);
    for (const auto& server: m_serversWatcher.servers())
        ui->selectServerMenuButton->addServer(server);

    connect(ui->addressOrSubnetMaskEdit,
        &InputField::textChanged,
        this,
        &NewDeviceAdditionDialog::handleAddressFieldTextChanged);
    connect(ui->portSpinBox,
        &QSpinBox::valueChanged,
        this,
        &NewDeviceAdditionDialog::handlePortValueChanged);

    ui->addressOrSubnetMaskEdit->setExternalControls(
        ui->addressOrSubnetMaskLabel, ui->addressOrSubnetMaskLabelHint);
    ui->addressOrSubnetMaskEdit->setUseWarningStyleForControl(false);
    ui->addressOrSubnetMaskLabelHint->setVisible(false);
    ui->addressOrSubnetMaskLabel->addHintLine(tr("Possible formats:"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("192.168.1.15"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("www.example.com:8080"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("http://example.com:7090"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("rtsp://example.com:554/video"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("udp://239.250.5.5:1234"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("192.168.1.1-192.168.1.20"));
    ui->addressOrSubnetMaskLabel->addHintLine(lit("192.168.1.1/24"));

    ui->widget->setFixedWidth(ui->addressOrSubnetMaskLabelLayout->minimumSize().width());
    installEventHandler(ui->serverChoosePanel, QEvent::PaletteChange, ui->serverChoosePanel,
        [this]()
        {
            setPaletteColor(ui->serverChoosePanel, QPalette::Window,
                palette().color(QPalette::Mid));
        });

    connect(ui->selectServerMenuButton, &QnChooseServerButton::currentServerChanged,
        this,
        &NewDeviceAdditionDialog::handleSelectedServerChanged);

    updateSelectedServerButtonVisibility();
    connect(&m_serversWatcher, &CurrentSystemServers::serversCountChanged,
        this,
        &NewDeviceAdditionDialog::updateSelectedServerButtonVisibility);
    connect(&m_serverStatusWatcher, &ServerOnlineStatusWatcher::statusChanged,
        this,
        &NewDeviceAdditionDialog::handleServerOnlineStateChanged);

    connect(
        this, &NewDeviceAdditionDialog::rejected,
        this,
        &NewDeviceAdditionDialog::handleDialogClosed);

    setupTable();

    setAddDevicesAccent(false);

    updateResultsWidgetState();

    setupSearchResultsPlaceholder();

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
            menu()->trigger(menu::SystemAdministrationAction,
                menu::Parameters().withArgument(
                    Qn::FocusTabRole, QnSystemAdministrationDialog::SecurityPage));
        });
}

void NewDeviceAdditionDialog::setAddDevicesAccent(bool canAddDevices)
{
    const auto accentButton = canAddDevices ? ui->addDevicesButton : ui->searchButton;
    const auto regularButton = accentButton == ui->addDevicesButton ? ui->searchButton : ui->addDevicesButton;

    regularButton->setDefault(false);
    resetButtonStyle(regularButton);

    accentButton->setDefault(true);
    setAccentStyle(accentButton);
}

bool NewDeviceAdditionDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::FocusIn)
    {
        const bool addDevicesFocus = object == ui->foundDevicesTable || object == ui->addDevicesButton;
        const bool newDevicesPresent = m_model && m_model->deviceCount(FoundDevicesModel::notPresentedState) > 0;
        setAddDevicesAccent(addDevicesFocus && newDevicesPresent);
    }
    return base_type::eventFilter(object, event);
}

void NewDeviceAdditionDialog::handleAddressFieldTextChanged(const QString& value)
{
    using namespace nx::vms::client::desktop::manual_device_addition;

    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);

    auto address = parseDeviceAddress(value);
    portIndex = address.portIndex;
    portLength = address.portLength;

    if (address.port)
        ui->portSpinBox->setValue(*address.port);
}

void NewDeviceAdditionDialog::handlePortValueChanged(int value)
{
    if (m_addressEditing)
        return;

    QScopedValueRollback<bool> rollback(m_addressEditing, true);

    if (portIndex && portLength)
    {
        auto address = ui->addressOrSubnetMaskEdit->text();
        auto port = QString::number(value);
        address.replace(*portIndex, *portLength, port);
        ui->addressOrSubnetMaskEdit->setText(address);
        *portLength = port.size();
    }
}

void NewDeviceAdditionDialog::handleTabClicked(int index)
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
}

void NewDeviceAdditionDialog::updateSelectedServerButtonVisibility()
{
    const bool visible = m_serversWatcher.serversCount() > 1;
    ui->serverChoosePanel->setVisible(visible);
}

void NewDeviceAdditionDialog::handleSelectedServerChanged(const QnMediaServerResourcePtr& previous)
{
    m_serverStatusWatcher.setServer(ui->selectServerMenuButton->currentServer());
    if (!previous)
        return;

    cleanUnfinishedSearches(previous);
    stopSearch();
}

void NewDeviceAdditionDialog::handleServerOnlineStateChanged()
{
    const bool online = m_serverStatusWatcher.isOnline();

    ui->serverOfflineAlertBar->setVisible(!online);
    ui->searchResultsStackedWidget->setEnabled(online);
    ui->searchPanel->setEnabled(online);

    if (!online && m_currentSearch)
        stopSearch();
}

void NewDeviceAdditionDialog::setupTable()
{
    const auto table = ui->foundDevicesTable;

    table->setCheckboxColumn(FoundDevicesModel::checkboxColumn);
    table->setItemDelegate(new FoundDevicesDelegate(table));
    table->setPersistentDelegateForColumn(FoundDevicesModel::presentedStateColumn,
        new PresentedStateDelegate(table));
}

void NewDeviceAdditionDialog::setupTableHeader()
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

void NewDeviceAdditionDialog::setupPortStuff(
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

void NewDeviceAdditionDialog::setupSearchResultsPlaceholder()
{
    QFont font;
    font.setPixelSize(16);
    ui->searchPlaceholderTitle->setFont(font);
    font.setWeight(QFont::Light);
    font.setPixelSize(12);
    ui->searchPlaceholderText->setFont(font);
    setPaletteColor(ui->searchPlaceholderTitle,
        QPalette::All,
        QPalette::WindowText,
        QPalette().color(QPalette::Light));
    setPaletteColor(ui->searchPlaceholderText,
        QPalette::All,
        QPalette::WindowText,
        QPalette().color(QPalette::Light));
}

void NewDeviceAdditionDialog::setServer(const QnMediaServerResourcePtr& value)
{
    if (!ui->selectServerMenuButton->setCurrentServer(value))
        stopSearch();
}

std::optional<int> NewDeviceAdditionDialog::port() const
{
    return std::nullopt;
}

QString NewDeviceAdditionDialog::password() const
{
    return {};
}

QString NewDeviceAdditionDialog::login() const
{
    return {};
}

void NewDeviceAdditionDialog::handleStartSearchClicked()
{
    if (m_currentSearch)
        stopSearch();

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

    m_model.reset(new FoundDevicesModel(ui->foundDevicesTable));
    m_sortModel->setSourceModel(m_model.data());

    updateResultsWidgetState();

    connect(m_currentSearch.data(), &ManualDeviceSearcher::statusChanged,
        this,
        &NewDeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model.data(), &FoundDevicesModel::rowsInserted,
        this,
        &NewDeviceAdditionDialog::updateResultsWidgetState);
    connect(m_model.data(), &FoundDevicesModel::rowsRemoved,
        this,
        &NewDeviceAdditionDialog::updateResultsWidgetState);

    connect(m_model.data(), &FoundDevicesModel::dataChanged,
        this,
        &NewDeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model.data(), &FoundDevicesModel::rowsInserted,
        this,
        &NewDeviceAdditionDialog::updateAddDevicesPanel);
    connect(m_model.data(), &FoundDevicesModel::rowsRemoved,
        this,
        &NewDeviceAdditionDialog::updateAddDevicesPanel);

    setupTableHeader();

    connect(m_currentSearch.data(), &ManualDeviceSearcher::devicesAdded,
        m_model.data(), &FoundDevicesModel::addDevices);
    connect(m_currentSearch.data(), &ManualDeviceSearcher::devicesRemoved,
        m_model.data(), &FoundDevicesModel::removeDevices);
}

void NewDeviceAdditionDialog::setDeviceAdded(const QString& physicalId)
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

void NewDeviceAdditionDialog::handleDeviceRemoved(const QString& physicalId)
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

void NewDeviceAdditionDialog::handleAddDevicesClicked()
{
    const auto server = ui->selectServerMenuButton->currentServer();
    if (!server || server->getStatus() != nx::vms::api::ResourceStatus::online || !m_model)
        return;
}

void NewDeviceAdditionDialog::showAdditionFailedDialog(const FakeResourceList& resources)
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

void NewDeviceAdditionDialog::stopSearch()
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

void NewDeviceAdditionDialog::cleanUnfinishedSearches(const QnMediaServerResourcePtr& server)
{
    const auto newEnd = std::remove_if(m_unfinishedSearches.begin(), m_unfinishedSearches.end(),
        [server](const SearcherPtr& searcher)
        {
            return searcher->server() == server;
        });
    m_unfinishedSearches.erase(newEnd, m_unfinishedSearches.end());
}

void NewDeviceAdditionDialog::removeUnfinishedSearch(const SearcherPtr& searcher)
{
    const auto newEnd = std::remove_if(m_unfinishedSearches.begin(), m_unfinishedSearches.end(),
        [searcher](const SearcherPtr& currentSearcher)
        {
            return searcher == currentSearcher;
        });
    m_unfinishedSearches.erase(newEnd, m_unfinishedSearches.end());
}

void NewDeviceAdditionDialog::handleSearchTypeChanged()
{
}

void NewDeviceAdditionDialog::handleDialogClosed()
{
    stopSearch();
    m_model.reset();
    updateResultsWidgetState();
}

QString NewDeviceAdditionDialog::progressMessage() const
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

void NewDeviceAdditionDialog::updateResultsWidgetState()
{
    static constexpr int kResultsPageIndex = 0;
    static constexpr int kPlaceholderPageIndex = 1;
    const bool empty = !m_model || !m_model->rowCount();

    ui->searchResultsStackedWidget->setCurrentIndex(
        empty ? kPlaceholderPageIndex : kResultsPageIndex);

    if (!m_currentSearch)
    {
        const auto pixmap =
            qnSkin->pixmap("placeholders/new_adding_devices_placeholder.svg", true, QSize(64, 64));
        ui->searchPlaceholderImage->setPixmap(qnSkin->maximumSizePixmap(pixmap));
        setPaletteColor(
            ui->searchPlaceholderTitle, QPalette::Text, core::colorTheme()->color("dark14"));
        setPaletteColor(
            ui->searchPlaceholderText, QPalette::Text, core::colorTheme()->color("dark14"));
        ui->searchPlaceholderTitle->show();
        ui->searchPlaceholderText->show();
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
        ui->searchPlaceholderImage->hide();
        ui->searchPlaceholderTitle->hide();
        ui->searchPlaceholderText->hide();
    }

    const bool showSearchProgressControls = m_currentSearch && m_currentSearch->searching();
    ui->searchProgressBar->setVisible(showSearchProgressControls);
    ui->httpsOnlyBar->setVisible(m_currentSearch && system()->globalSettings()->useHttpsOnlyForCameras()
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

void NewDeviceAdditionDialog::updateAddDevicesPanel()
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

void NewDeviceAdditionDialog::showAddDevicesButton(const QString& buttonText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesButtonPage);
    ui->addDevicesButton->setText(buttonText);
    ui->foundDevicesTable->setFocus();
}

void NewDeviceAdditionDialog::showAddDevicesPlaceholder(const QString& placeholderText)
{
    ui->addDevicesStackedWidget->setCurrentWidget(ui->addDevicesPlaceholderPage);
    ui->addDevicesPlaceholder->setText(placeholderText);
    setAddDevicesAccent(false);
}

} // namespace nx::vms::client::desktop

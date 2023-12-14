// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_synchronization_widget.h"
#include "ui_time_synchronization_widget.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QTimer>

#include <common/common_module.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/algorithm/index_of.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/resource/server.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/time/formatter.h>
#include <nx/vms/time_sync/time_sync_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <ui/common/read_only.h>
#include <utils/common/synctime.h>

#include "../delegates/time_synchronization_servers_delegate.h"
#include "../flux/time_synchronization_widget_state.h"
#include "../flux/time_synchronization_widget_store.h"
#include "../models/time_synchronization_servers_model.h"
#include "../watchers/time_synchronization_server_state_watcher.h"
#include "../watchers/time_synchronization_server_time_watcher.h"

namespace nx::vms::client::desktop {

using State = TimeSynchronizationWidgetState;
using Model = TimeSynchronizationServersModel;
using TimeWatcher = TimeSynchronizationServerTimeWatcher;
using StateWatcher = TimeSynchronizationServerStateWatcher;

namespace {

static constexpr int kTimeFontPixelSize = 40;
static constexpr auto kTimeFontWeight = QFont::Normal;
static constexpr int kDateFontPixelSize = 14;
static constexpr auto kDateFontWeight = QFont::Bold;
static constexpr int kZoneFontPixelSize = 14;
static constexpr auto kZoneFontWeight = QFont::Normal;

static const QColor kDark10Color = "#303F47";
static const core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kDark10Color, "dark10"}}}
};

} // namespace

TimeSynchronizationWidget::TimeSynchronizationWidget(
    api::SaveableSystemSettings* editableSystemSettings,
    QWidget* parent)
    :
    base_type(editableSystemSettings, parent),
    ui(new Ui::TimeSynchronizationWidget),
    m_store(new TimeSynchronizationWidgetStore(this)),
    m_serversModel(new Model(this)),
    m_timeWatcher(new TimeSynchronizationServerTimeWatcher(m_store, this)),
    m_stateWatcher(new TimeSynchronizationServerStateWatcher(m_store, this)),
    m_delegate(new TimeSynchronizationServersDelegate(this))
{
    setupUi();

    connect(m_store, &TimeSynchronizationWidgetStore::stateChanged, this,
        &TimeSynchronizationWidget::loadState);

    connect(ui->syncWithInternetCheckBox, &QCheckBox::clicked, m_store,
        &TimeSynchronizationWidgetStore::setSyncTimeWithInternet);

    const auto handleDisableClick =
        [this]
        {
            ui->disableSyncRadioButton->setChecked(true);
            m_store->disableSync();
        };
    connect(ui->disableSyncRadioButton, &QRadioButton::clicked, this, handleDisableClick);

    auto updateDelegate =
        [this]
        {
            setBaseOffsetIndex(-1);
        };

    connect(ui->syncWithInternetCheckBox, &QCheckBox::clicked, this, updateDelegate);
    connect(ui->disableSyncRadioButton, &QRadioButton::clicked, this, updateDelegate);

    auto clearHovered =
        [this]
        {
            switch (m_store->state().status)
            {
                case State::Status::notSynchronized:
                case State::Status::noInternetConnection:
                case State::Status::selectedServerIsOffline:
                    setBaseOffsetIndex(-1);
                default:
                    break;
            }
        };
    auto setHovered =
        [this](int row)
        {
            switch (m_store->state().status)
            {
                case State::Status::notSynchronized:
                case State::Status::noInternetConnection:
                case State::Status::selectedServerIsOffline:
                    setBaseOffsetIndex(row);
                default:
                    break;
            }
        };

    connect(ui->serversTable->hoverTracker(), &ItemViewHoverTracker::rowEnter, this, setHovered);
    connect(ui->serversTable->hoverTracker(), &ItemViewHoverTracker::rowLeave, this, clearHovered);

    const auto selectServer =
        [this](const QModelIndex& index)
        {
            const QnUuid& serverId = index.data(Model::ServerIdRole).value<QnUuid>();
            // volatile: index is updated when we call m_store->selectServer,
            // so we need to be sure that compiler won't move .row() call below
            volatile const int row = index.row();
            if (!serverId.isNull())
            {
                m_store->selectServer(serverId);
                if (m_store->state().status ==  State::Status::synchronizedWithSelectedServer)
                    setBaseOffsetIndex(row);
            }
        };

    connect(m_serversModel, &Model::serverSelected, this, selectServer);
    connect(ui->serversTable, &TableView::pressed, this, selectServer);

    const auto updateTime =
        [this]
        {
            m_store->setElapsedTime(m_timeWatcher->elapsedTime());
        };

    auto timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, updateTime);
    timer->start();

    connect(
        connection()->timeSynchronizationManager().get(),
        &nx::vms::time::TimeSyncManager::timeChanged,
        m_timeWatcher,
        &TimeSynchronizationServerTimeWatcher::forceUpdate);

    updateTime();
}

TimeSynchronizationWidget::~TimeSynchronizationWidget()
{
}

void TimeSynchronizationWidget::loadDataToUi()
{
    // Create state.
    QList<State::ServerInfo> servers;
    for (const auto& resource: resourcePool()->servers())
    {
        const auto server = resource.dynamicCast<core::ServerResource>();
        if (!NX_ASSERT(server))
            continue;

        State::ServerInfo serverInfo;
        serverInfo.id = server->getId();
        serverInfo.name = server->getName();
        serverInfo.ipAddress = QnResourceDisplayInfo(server).host();
        serverInfo.online = server->getStatus() == nx::vms::api::ResourceStatus::online;
        serverInfo.hasInternet = server->hasInternetAccess();
        serverInfo.timeZoneOffset = server->timeZoneInfo().utcOffset;
        servers.push_back(serverInfo);
    }

    m_store->initialize(
        systemSettings()->isTimeSynchronizationEnabled(),
        systemSettings()->primaryTimeServer(),
        servers);

    m_store->setBaseTime(qnSyncTime->value());

    const auto& state = m_store->state();
    if (state.status ==  State::Status::synchronizedWithSelectedServer)
    {
        const auto idx = nx::utils::algorithm::index_of(state.servers,
            [&state](const State::ServerInfo& serverInfo)
            {
                return serverInfo.id == state.primaryServer;
            });
        setBaseOffsetIndex(idx);
    }
    else
    {
        setBaseOffsetIndex(-1);
    }
}

void TimeSynchronizationWidget::applyChanges()
{
    if (!hasChanges())
        return;

    const auto& state = m_store->state();
    editableSystemSettings->timeSynchronizationEnabled = state.enabled;
    editableSystemSettings->primaryTimeServer = state.primaryServer;

    m_store->applyChanges();
}

bool TimeSynchronizationWidget::hasChanges() const
{
    return !m_store->state().readOnly && m_store->state().hasChanges;
}

void TimeSynchronizationWidget::setReadOnlyInternal(bool readOnly)
{
    m_store->setReadOnly(readOnly);
}

void TimeSynchronizationWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    m_timeWatcher->start();
}

void TimeSynchronizationWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    m_timeWatcher->stop();
}

void TimeSynchronizationWidget::setupUi()
{
    ui->setupUi(this);
    setHelpTopic(this, HelpTopic::Id::Administration_TimeSynchronization);

    ui->vmsTimeCaptionLabel->setHint(tr("Time, common and synchronized between all servers. "
        "Can be different with OS time on any particular server."));

    ui->syncWithInternetCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->syncWithInternetCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->placeholderImageLabel->setPixmap(
        qnSkin->icon("placeholders/time_placeholder.svg", kIconSubstitutions)
            .pixmap(QSize(128, 128)));

    QFont font;
    font.setPixelSize(kTimeFontPixelSize);
    font.setWeight(kTimeFontWeight);
    ui->timeLabel->setFont(font);
    ui->timeLabel->setForegroundRole(QPalette::Text);

    font.setPixelSize(kDateFontPixelSize);
    font.setWeight(kDateFontWeight);
    ui->dateLabel->setFont(font);
    ui->dateLabel->setForegroundRole(QPalette::Light);

    font.setPixelSize(kZoneFontPixelSize);
    font.setWeight(kZoneFontWeight);
    ui->zoneLabel->setFont(font);
    ui->zoneLabel->setForegroundRole(QPalette::Light);

    QPalette palette(ui->timePlaceholderLabel->palette());
    palette.setColor(QPalette::WindowText, core::colorTheme()->color("dark14"));
    font.setPixelSize(kTimeFontPixelSize);
    font.setWeight(kTimeFontWeight);
    ui->timePlaceholderLabel->setFont(font);
    ui->timePlaceholderLabel->setPalette(palette);

    auto* sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(m_serversModel);
    ui->serversTable->setModel(sortModel);
    ui->serversTable->setProperty(style::Properties::kItemViewRadioButtons, true);
    ui->serversTable->setItemDelegate(m_delegate);

    auto hHeader = ui->serversTable->horizontalHeader();
    hHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    hHeader->setSectionResizeMode(Model::NameColumn, QHeaderView::Stretch);
    hHeader->setSectionsClickable(false);

    auto vHeader = ui->serversTable->verticalHeader();
    vHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
}

void TimeSynchronizationWidget::setBaseOffsetIndex(int row)
{
    m_delegate->setBaseRow(row);
    ui->serversTable->update();
}

void TimeSynchronizationWidget::loadState(const State& state)
{
    using ::setReadOnly;

    const auto vmsDateTime = QDateTime::fromMSecsSinceEpoch(
        duration_cast<milliseconds>(state.baseTime + state.elapsedTime).count(),
        Qt::OffsetFromUTC,
        duration_cast<seconds>(state.commonTimezoneOffset).count());

    const bool isSyncWithInternet = state.syncTimeWithInternet();

    QString detailsText;
    bool showWarning = false;
    switch (state.status)
    {
        case State::Status::synchronizedWithInternet:
            detailsText = tr("Synchronized with the Internet");
            break;
        case State::Status::synchronizedWithSelectedServer:
            detailsText = tr("Synchronized with the local time at the selected server");
            break;
        case State::Status::notSynchronized:
            detailsText = tr("Not synchronized. Each server uses its own local time.");
            break;
        case State::Status::singleServerLocalTime:
            detailsText = tr("Equal to the server local time");
            break;
        case State::Status::noInternetConnection:
            detailsText = tr("No Internet connection. Time is not being synchronized.");
            showWarning = true;
            break;
        case State::Status::selectedServerIsOffline:
            detailsText = tr("Time Server is offline. Time is not being synchronized.");
            showWarning = true;
            break;
        default:
            NX_ASSERT(false, "All states must be handled");
            break;
    }
    ui->statusLabel->setText(detailsText);
    ui->statusLabel->setWarningStyle(showWarning);

    ui->timeLabel->setText(nx::vms::time::toString(vmsDateTime.time()));
    ui->dateLabel->setText(nx::vms::time::toString(vmsDateTime.date()));
    ui->zoneLabel->setText(vmsDateTime.timeZoneAbbreviation());

    switch (state.status)
    {
        case State::Status::synchronizedWithInternet:
        case State::Status::synchronizedWithSelectedServer:
        case State::Status::singleServerLocalTime:
            ui->vmsTimeWidget->setCurrentWidget(ui->timePage);
            break;
        case State::Status::notSynchronized:
        case State::Status::noInternetConnection:
        case State::Status::selectedServerIsOffline:
            ui->vmsTimeWidget->setCurrentWidget(ui->placeholderPage);
            break;
        default:
            NX_ASSERT(false, "All states must be handled");
            break;
    }

    ui->detailsWidget->setCurrentWidget(state.status == State::Status::synchronizedWithInternet
        ? ui->synchronizedPage
        : ui->serversTablePage);

    ui->syncWithInternetCheckBox->setChecked(isSyncWithInternet);
    setReadOnly(ui->syncWithInternetCheckBox, state.readOnly);

    ui->disableSyncRadioButton->setChecked(!state.enabled);
    setReadOnly(ui->disableSyncRadioButton, state.readOnly);

    switch (state.status)
    {
        case State::Status::singleServerLocalTime:
            ui->disableSyncRadioButton->hide();
            ui->serversTable->hideColumn(Model::CheckboxColumn);
            ui->serversTable->hideColumn(Model::VmsTimeColumn);
            break;
        case State::Status::noInternetConnection:
            ui->disableSyncRadioButton->hide();
            ui->serversTable->hideColumn(Model::CheckboxColumn);
            ui->serversTable->showColumn(Model::VmsTimeColumn);
            break;
        default:
            ui->disableSyncRadioButton->show();
            ui->serversTable->showColumn(Model::CheckboxColumn);
            ui->serversTable->showColumn(Model::VmsTimeColumn);
            break;
    }

    m_serversModel->loadState(state);

    const bool newHasChanges = hasChanges();
    if (m_hasChanges != newHasChanges)
    {
        m_hasChanges = newHasChanges;
        emit hasChangesChanged();
    }
}

} // namespace nx::vms::client::desktop

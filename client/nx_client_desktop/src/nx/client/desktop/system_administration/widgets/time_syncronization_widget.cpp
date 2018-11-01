#include "time_syncronization_widget.h"
#include "ui_time_syncronization_widget.h"

#include <QtCore/QTimer>
#include <QtWidgets/QStyledItemDelegate>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <translation/datetime_formatter.h>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <utils/common/synctime.h>

#include "../redux/time_syncronization_widget_store.h"
#include "../redux/time_syncronization_widget_state.h"
#include "../models/time_synchronization_servers_model.h"
#include <ui/style/skin.h>

namespace nx::client::desktop {

using State = TimeSynchronizationWidgetState;
using Model = TimeSynchronizationServersModel;

namespace {

static constexpr int kTimeFontPixelSize = 40;
static constexpr int kTimeFontWeight = QFont::Normal;
static constexpr int kDateFontPixelSize = 14;
static constexpr int kDateFontWeight = QFont::Bold;
static constexpr int kZoneFontPixelSize = 14;
static constexpr int kZoneFontWeight = QFont::Normal;
static constexpr int kMinimumDateTimeWidth = 84;

QDateTime dateTimeFromMSecs(std::chrono::milliseconds value)
{
    QDateTime result;
    result.setTimeSpec(Qt::UTC);
    result.setMSecsSinceEpoch(value.count());

    const auto offsetFromUtc = result.offsetFromUtc();
    result.setTimeSpec(Qt::OffsetFromUTC);
    result.setOffsetFromUtc(offsetFromUtc);
    return result;
}

class TimeServerDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    TimeServerDelegate(QObject* parent = nullptr): base_type(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QSize size = base_type::sizeHint(option, index);
        switch (index.column())
        {
            case Model::DateColumn:
            case Model::OsTimeColumn:
            case Model::VmsTimeColumn:
                size.setWidth(std::max(size.width(), kMinimumDateTimeWidth));
                break;

            default:
                break;
        }
        return size;
    }
};

} // namespace

TimeSynchronizationWidget::TimeSynchronizationWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimeSynchronizationWidget),
    m_store(new TimeSynchronizationWidgetStore(this)),
    m_serversModel(new Model(this))
{
    setupUi();

    connect(m_store, &TimeSynchronizationWidgetStore::stateChanged, this,
        &TimeSynchronizationWidget::loadState);

    connect(ui->syncWithInternetCheckBox, &QCheckBox::clicked, m_store,
        &TimeSynchronizationWidgetStore::setSyncTimeWithInternet);

    connect(ui->disableSyncRadioButton, &QRadioButton::clicked, m_store,
        &TimeSynchronizationWidgetStore::disableSync);

    //connect(m_serversModel, &Model::serverSelected, m_store,
    //    &TimeSynchronizationWidgetStore::selectServer);

    const auto updateTime =
        [this]
        {
            m_store->setVmsTime(std::chrono::milliseconds(qnSyncTime->currentMSecsSinceEpoch()));
        };

    auto timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, updateTime);
    timer->start();

    connect(qnSyncTime, &QnSyncTime::timeChanged, this, updateTime);

    updateTime();
}

TimeSynchronizationWidget::~TimeSynchronizationWidget()
{
}

void TimeSynchronizationWidget::loadDataToUi()
{
    // Create state.
    QList<State::ServerInfo> servers;
    for (const auto& server: resourcePool()->getAllServers(Qn::AnyStatus))
    {
        State::ServerInfo serverInfo;
        serverInfo.id = server->getId();
        serverInfo.name = server->getName();
        serverInfo.ipAddress = server->getUrl();
        serverInfo.online = server->getStatus() == Qn::Online;
        serverInfo.hasInternet = server->getServerFlags().testFlag(
            vms::api::ServerFlag::SF_HasPublicIP);
        servers.push_back(serverInfo);
    }

    m_store->initialize(
        qnGlobalSettings->isTimeSynchronizationEnabled(),
        qnGlobalSettings->primaryTimeServer(),
        servers);
}

void TimeSynchronizationWidget::applyChanges()
{
    if (!hasChanges())
        return;

    const auto& state = m_store->state();

    qnGlobalSettings->setTimeSynchronizationEnabled(state.enabled);
    qnGlobalSettings->setPrimaryTimeServer(state.primaryServer);
    qnGlobalSettings->synchronizeNow();
}

bool TimeSynchronizationWidget::hasChanges() const
{
    return !m_store->state().readOnly && m_store->state().hasChanges;
}

void TimeSynchronizationWidget::setReadOnlyInternal(bool readOnly)
{
    m_store->setReadOnly(readOnly);
}

void TimeSynchronizationWidget::setupUi()
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_TimeSynchronization_Help);

    ui->syncWithInternetCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->syncWithInternetCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->placeholderImageLabel->setPixmap(qnSkin->pixmap("placeholders/time_placeholder.png"));

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

    auto* sortModel = new QSortFilterProxyModel(this);
    sortModel->setSourceModel(m_serversModel);
    ui->serversTable->setModel(sortModel);
    ui->serversTable->setProperty(style::Properties::kItemViewRadioButtons, true);
    ui->serversTable->setItemDelegate(new TimeServerDelegate(this));

    auto header = ui->serversTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(Model::NameColumn, QHeaderView::Stretch);
    header->setSectionsClickable(false);
    //ui->serversTable->setItemDelegateForColumn(Model::NameColumn, new QnResourceItemDelegate(this));
}

void TimeSynchronizationWidget::loadState(const State& state)
{
    using ::setReadOnly;

    const auto vmsDateTime = dateTimeFromMSecs(state.vmsTime);
    const bool isSyncWithInternet = state.syncTimeWithInternet();

    QString detailsText;
    bool showWarning = false;
    switch (state.status)
    {
        case State::Status::synchronizedWithInternet:
            detailsText = tr("Synchronized with the Internet.");
            break;
        case State::Status::synchronizedWithSelectedServer:
            detailsText = tr("Synchronized with the local time at the selected server.");
            break;
        case State::Status::notSynchronized:
            detailsText = tr("Not synchronized. Each server uses its own local time.");
            break;
        case State::Status::singleServerLocalTime:
            detailsText = tr("Equal to the server local time.");
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
            break;
    }
    ui->statusLabel->setText(detailsText);
    ui->statusLabel->setWarningStyle(showWarning);

    ui->timeLabel->setText(datetime::toString(vmsDateTime.time()));
    ui->dateLabel->setText(datetime::toString(vmsDateTime.date()));
    ui->zoneLabel->setText(vmsDateTime.timeZoneAbbreviation());

    ui->detailsWidget->setCurrentWidget(state.status == State::Status::synchronizedWithInternet
        ? ui->synchronizedPage
        : ui->serversTablePage);

    ui->syncWithInternetCheckBox->setChecked(isSyncWithInternet);
    setReadOnly(ui->syncWithInternetCheckBox, state.readOnly);

    ui->disableSyncRadioButton->setChecked(!state.enabled);
    setReadOnly(ui->disableSyncRadioButton, state.readOnly);

    m_serversModel->loadState(state);

    emit hasChangesChanged();
}

} // namespace nx::client::desktop

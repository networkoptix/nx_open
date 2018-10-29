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

namespace {

static const int kTimeFontSizePixels = 40;
static const int kTimeFontWeight = QFont::Normal;
static const int kDateFontSizePixels = 14;
static const int kDateFontWeight = QFont::Bold;
static const int kZoneFontSizePixels = 14;
static const int kZoneFontWeight = QFont::Normal;
static const int kMinimumDateTimeWidth = 84;

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
    virtual QSize sizeHint(const QStyleOptionViewItem& option,
        const QModelIndex& index) const override
    {
        QSize size = base_type::sizeHint(option, index);
        /*
        switch (index.column())
        {
            case QnTimeServerSelectionModel::DateColumn:
            case QnTimeServerSelectionModel::TimeColumn:
                size.setWidth(qMax(size.width(), kMinimumDateTimeWidth));
                break;

            default:
                break;
        }
        */
        return size;
    }
};

} // namespace

namespace nx::client::desktop {

using State = TimeSynchronizationWidgetState;

TimeSynchronizationWidget::TimeSynchronizationWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::TimeSynchronizationWidget),
    m_store(new TimeSynchronizationWidgetStore(this))
{
    setupUi();

    connect(m_store, &TimeSynchronizationWidgetStore::stateChanged, this,
        &TimeSynchronizationWidget::loadState);

    connect(ui->syncWithInternetCheckBox, &QCheckBox::clicked, m_store,
        &TimeSynchronizationWidgetStore::setSyncTimeWithInternet);

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

    QFont font;
    font.setPixelSize(kTimeFontSizePixels);
    font.setWeight(kTimeFontWeight);
    ui->timeLabel->setFont(font);
    ui->timeLabel->setForegroundRole(QPalette::Text);

    font.setPixelSize(kDateFontSizePixels);
    font.setWeight(kDateFontWeight);
    ui->dateLabel->setFont(font);
    ui->dateLabel->setForegroundRole(QPalette::Light);

    font.setPixelSize(kZoneFontSizePixels);
    font.setWeight(kZoneFontWeight);
    ui->zoneLabel->setFont(font);
    ui->zoneLabel->setForegroundRole(QPalette::Light);
}

void TimeSynchronizationWidget::loadState(const TimeSynchronizationWidgetState& state)
{
    using ::setReadOnly;

    const auto vmsDateTime = dateTimeFromMSecs(state.vmsTime);
    const bool isSyncWithInternet = state.syncTimeWithInternet();

    ui->timeLabel->setText(datetime::toString(vmsDateTime.time()));
    ui->dateLabel->setText(datetime::toString(vmsDateTime.date()));
    ui->zoneLabel->setText(vmsDateTime.timeZoneAbbreviation());

    ui->detailsWidget->setCurrentWidget(state.status == State::Status::synchronizedWithInternet
        ? ui->placeholderPage
        : ui->serversTablePage);

    ui->syncWithInternetCheckBox->setChecked(isSyncWithInternet);
    setReadOnly(ui->syncWithInternetCheckBox, state.readOnly);


}

} // namespace nx::client::desktop

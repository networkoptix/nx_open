#include "time_server_selection_widget.h"
#include "ui_time_server_selection_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>
#include <translation/datetime_formatter.h>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/style/helper.h>
#include <ui/models/time_server_selection_model.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <nx/vms/client/desktop/system_health/system_internet_access_watcher.h>
#include <nx/utils/string.h>
#include <nx/vms/api/types/connection_types.h>
#include <utils/common/synctime.h>

//#define QN_TIME_SERVER_SELECTION_DEBUG

#ifdef QN_TIME_SERVER_SELECTION_DEBUG
#define PRINT_DEBUG(MSG) qDebug() << MSG
#else
#define PRINT_DEBUG(MSG)
#endif

using namespace nx;
using namespace nx::vms::client::desktop;

namespace {

static const int kTimeFontSizePixels = 40;
static const int kTimeFontWeight = QFont::Normal;
static const int kDateFontSizePixels = 14;
static const int kDateFontWeight = QFont::Bold;
static const int kZoneFontSizePixels = 14;
static const int kZoneFontWeight = QFont::Normal;
static const int kMinimumDateTimeWidth = 84;

class QnSortServersByNameProxyModel: public QSortFilterProxyModel
{
public:
    explicit QnSortServersByNameProxyModel(QObject* parent = nullptr):
        QSortFilterProxyModel(parent)
    {
    }

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const
    {
        const auto leftStr = left.sibling(left.row(), QnTimeServerSelectionModel::NameColumn)
            .data(Qt::DisplayRole).toString();

        const auto rightStr = right.sibling(left.row(), QnTimeServerSelectionModel::NameColumn)
            .data(Qt::DisplayRole).toString();

        return nx::utils::naturalStringCompare(leftStr, rightStr, Qt::CaseInsensitive) < 0;
    }
};

class QnTimeServerDelegate: public QStyledItemDelegate
{
    using base_type = QStyledItemDelegate;

public:
    QnTimeServerDelegate(QObject* parent = nullptr): base_type(parent) {}

    virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize size = base_type::sizeHint(option, index);

        switch (index.column())
        {
            case QnTimeServerSelectionModel::DateColumn:
            case QnTimeServerSelectionModel::TimeColumn:
                size.setWidth(qMax(size.width(), kMinimumDateTimeWidth));
                break;

            default:
                break;
        }

        return size;
    }
};

} // namespace

QnTimeServerSelectionWidget::QnTimeServerSelectionWidget(QWidget *parent /* = NULL*/) :
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::TimeServerSelectionWidget),
    m_model(new QnTimeServerSelectionModel(this))
{
    ui->setupUi(this);
    setHelpTopic(this, Qn::Administration_TimeSynchronization_Help);

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

    auto* sortModel = new QnSortServersByNameProxyModel(this);
    sortModel->setSourceModel(m_model);

    ui->serversTable->setModel(sortModel);
    ui->serversTable->setProperty(style::Properties::kItemViewRadioButtons, true);
    ui->serversTable->setItemDelegate(new QnTimeServerDelegate(this));
    ui->serversTable->setItemDelegateForColumn(
        QnTimeServerSelectionModel::NameColumn, new QnResourceItemDelegate(this));

    auto header = ui->serversTable->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setSectionResizeMode(QnTimeServerSelectionModel::NameColumn, QHeaderView::Stretch);
    header->setSectionsClickable(false);

    connect(m_model, &QnTimeServerSelectionModel::dataChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

    static constexpr int kUpdateTimeMs = 1000;
    static constexpr int kUpdateTimeOffsetMs = 5000;
    auto updateTime =
        [this]()
        {
            static const int k = kUpdateTimeOffsetMs / kUpdateTimeMs;
            if (isVisible())
            {
                if (m_timeCounter % k == 0)
                    m_model->updateTimeOffset();
                ++m_timeCounter;
                this->updateTime();
            }
            else
            {
                m_timeCounter = 0;
            }
        };

    connect(qnSyncTime, &QnSyncTime::timeChanged, this, updateTime);

    auto timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, updateTime);
    timer->start();

    const auto onSyncWithInternetCheckboxToggled =
        [this](bool checked)
        {
            ui->serversTable->setColumnHidden(QnTimeServerSelectionModel::CheckboxColumn,
                checked);

            if (!checked && m_model->selectedServer().isNull())
            {
                auto model = ui->serversTable->model();
                const auto topIndex = model->index(0, QnTimeServerSelectionModel::CheckboxColumn);
                model->setData(topIndex, Qt::Checked, Qt::CheckStateRole);
            }

            updateDescription();
            updateAlert();

            emit hasChangesChanged();
        };

    connect(ui->syncWithInternetCheckBox, &QAbstractButton::toggled,
        this, onSyncWithInternetCheckboxToggled);

    connect(commonModule()->instance<SystemInternetAccessWatcher>(),
        &SystemInternetAccessWatcher::internetAccessChanged,
        this, &QnTimeServerSelectionWidget::updateAlert);

    connect(globalSettings(), &QnGlobalSettings::timeSynchronizationSettingsChanged, this,
        &QnTimeServerSelectionWidget::loadDataToUi);

    onSyncWithInternetCheckboxToggled(ui->syncWithInternetCheckBox->isChecked());
}

QnTimeServerSelectionWidget::~QnTimeServerSelectionWidget()
{
}

void QnTimeServerSelectionWidget::loadDataToUi()
{
    PRINT_DEBUG("provide selected server to model:");
    m_model->setSelectedServer(globalSettings()->primaryTimeServer());
    ui->syncWithInternetCheckBox->setChecked(globalSettings()->primaryTimeServer().isNull());
    updateTime();
}

void QnTimeServerSelectionWidget::applyChanges()
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    auto globalSettings = commonModule()->globalSettings();

    if (ui->syncWithInternetCheckBox->isChecked())
    {
        globalSettings->setPrimaryTimeServer(QnUuid());
        globalSettings->synchronizeNow();
        return;
    }

    PRINT_DEBUG("forcing selected server to " + m_model->selectedServer().toByteArray());

    globalSettings->setPrimaryTimeServer(m_model->selectedServer());
    globalSettings->synchronizeNow();
}

bool QnTimeServerSelectionWidget::hasChanges() const
{
    const bool syncWithInternet = ui->syncWithInternetCheckBox->isChecked();
    const bool syncWithInternetInSettings = globalSettings()->primaryTimeServer().isNull();
    if (syncWithInternetInSettings != syncWithInternet)
        return true;

    return syncWithInternet
        ? false
        : m_model->selectedServer() != globalSettings()->primaryTimeServer();
}

void QnTimeServerSelectionWidget::updateTime()
{
    m_model->updateTime();

    QDateTime syncTime;
    if (m_model->sameTimezone())
        syncTime.setTimeSpec(Qt::LocalTime);
    else
        syncTime.setTimeSpec(Qt::UTC);

    syncTime.setMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch());

    auto offsetFromUtc = syncTime.offsetFromUtc();
    syncTime.setTimeSpec(Qt::OffsetFromUTC);
    syncTime.setOffsetFromUtc(offsetFromUtc);

    ui->timeLabel->setText(datetime::toString(syncTime.time()));
    ui->dateLabel->setText(datetime::toString(syncTime.date()));
    ui->zoneLabel->setText(syncTime.timeZoneAbbreviation());

    ui->stackedWidget->setCurrentWidget(ui->timePage);
}

void QnTimeServerSelectionWidget::updateDescription()
{
    ui->descriptionLabel->setText(ui->syncWithInternetCheckBox->isChecked()
        ? tr("VMS time is synchronized with the Internet and does not depend on local time on servers.")
        : tr("VMS time is synchronized with local time on the selected server and does not depend on local time on other servers."));
}

void QnTimeServerSelectionWidget::updateAlert()
{
    const bool showAlert = ui->syncWithInternetCheckBox->isChecked()
        && !commonModule()->instance<SystemInternetAccessWatcher>()->systemHasInternetAccess();

    ui->alertBar->setText(showAlert
        ? tr("No server has Internet access. Time is not being synchronized.")
        : QString());
}

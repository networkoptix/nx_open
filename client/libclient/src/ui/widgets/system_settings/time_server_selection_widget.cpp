#include "time_server_selection_widget.h"
#include "ui_time_server_selection_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/global_settings.h>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>

#include <nx_ec/data/api_runtime_data.h>

#include <ui/delegates/resource_item_delegate.h>
#include <ui/style/helper.h>
#include <ui/models/time_server_selection_model.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/synctime.h>

//#define QN_TIME_SERVER_SELECTION_DEBUG

#ifdef QN_TIME_SERVER_SELECTION_DEBUG
#define PRINT_DEBUG(MSG) qDebug() << MSG
#else
#define PRINT_DEBUG(MSG)
#endif

namespace {

static const int kTimeFontSizePixels = 40;
static const int kTimeFontWeight = QFont::Normal;
static const int kDateFontSizePixels = 14;
static const int kDateFontWeight = QFont::Bold;
static const int kZoneFontSizePixels = 14;
static const int kZoneFontWeight = QFont::Normal;
static const int kMinimumDateTimeWidth = 84;

class QnSortServersByPriorityProxyModel: public QSortFilterProxyModel
{
public:
    explicit QnSortServersByPriorityProxyModel(QObject* parent = nullptr):
        QSortFilterProxyModel(parent)
    {
    }

protected:
    virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const
    {
        return left.data(Qn::PriorityRole).toULongLong() < right.data(Qn::PriorityRole).toULongLong();
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

    ui->syncWithInternetCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->syncWithInternetCheckBox->setForegroundRole(QPalette::ButtonText);
    connect(ui->syncWithInternetCheckBox, &QAbstractButton::toggled, this,
        [this](bool checked)
        {
            ui->serversTable->setEnabled(!checked);
        });
    ui->serversTable->setEnabled(!ui->syncWithInternetCheckBox->isChecked());

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

    QnSortServersByPriorityProxyModel* sortModel = new QnSortServersByPriorityProxyModel(this);
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

    auto updateTime =
        [this]()
        {
            if (isVisible())
                this->updateTime();
        };

    connect(qnSyncTime, &QnSyncTime::timeChanged, this, updateTime);

    auto timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, updateTime);
    timer->start();
}

QnTimeServerSelectionWidget::~QnTimeServerSelectionWidget()
{
}

void QnTimeServerSelectionWidget::loadDataToUi()
{
    PRINT_DEBUG("provide selected server to model:");
    m_model->setSelectedServer(selectedServer());
    ui->syncWithInternetCheckBox->setChecked(qnGlobalSettings->isSynchronizingTimeWithInternet());
    updateTime();
}

void QnTimeServerSelectionWidget::applyChanges()
{
    auto connection = commonModule()->ec2Connection();
    if (!connection)
        return;

    PRINT_DEBUG("forcing selected server to " + m_model->selectedServer().toByteArray());
    auto timeManager = connection->getTimeManager(Qn::kSystemAccess);
    timeManager->forcePrimaryTimeServer(m_model->selectedServer(), this,
        [this](int handle, ec2::ErrorCode errCode)
        {
            Q_UNUSED(handle);
            Q_UNUSED(errCode);  //suppress warning in the release code
            PRINT_DEBUG("forcing selected server finished with result " + ec2::toString(errCode).toUtf8());
        });

    qnGlobalSettings->setSynchronizingTimeWithInternet(ui->syncWithInternetCheckBox->isChecked());
    qnGlobalSettings->synchronizeNow();
}

bool QnTimeServerSelectionWidget::hasChanges() const
{
    PRINT_DEBUG("checking if the widget has changes " + m_model->selectedServer().toByteArray() + " vs...");
    return m_model->selectedServer() != selectedServer();
}

QnUuid QnTimeServerSelectionWidget::selectedServer() const
{
    PRINT_DEBUG("check selected server by runtime info");

    for (const auto& runtimeInfo : runtimeInfoManager()->items()->getItems())
    {
        if (runtimeInfo.data.peer.peerType != Qn::PT_Server)
            continue;

        if (!m_model->isSelected(runtimeInfo.data.serverTimePriority))
            continue;

        PRINT_DEBUG("selected server " + runtimeInfo.uuid.toByteArray());
        return runtimeInfo.uuid;
    }

    PRINT_DEBUG("no selected server found");
    return QnUuid();
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

    ui->timeLabel->setText(syncTime.toString(lit("HH:mm:ss")));
    ui->dateLabel->setText(syncTime.toString(lit("dd/MM/yyyy")));
    ui->zoneLabel->setText(syncTime.timeZoneAbbreviation());

    ui->stackedWidget->setCurrentWidget(ui->timePage);
}

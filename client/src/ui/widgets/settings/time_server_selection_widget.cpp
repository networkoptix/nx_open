#include "time_server_selection_widget.h"
#include "ui_time_server_selection_widget.h"

#include <QtCore/QSortFilterProxyModel>

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <nx_ec/data/api_runtime_data.h>

#include <ui/models/time_server_selection_model.h>

#include <utils/common/synctime.h>

namespace {

    class QnSortServersByPriorityProxyModel: public QSortFilterProxyModel {
    public:
        explicit QnSortServersByPriorityProxyModel(QObject *parent = 0): 
            QSortFilterProxyModel(parent) {}

    protected:
        virtual bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
            return left.data(Qn::PriorityRole).toULongLong() < right.data(Qn::PriorityRole).toULongLong();
        }
    };

}

#ifdef _DEBUG
    #define QN_TIME_SERVER_SELECTION_DEBUG
#endif

#ifdef QN_TIME_SERVER_SELECTION_DEBUG
#define PRINT_DEBUG(MSG) qDebug() << MSG
#else
#define PRINT_DEBUG(MSG) 
#endif 

QnTimeServerSelectionWidget::QnTimeServerSelectionWidget(QWidget *parent /*= NULL*/):
    QnAbstractPreferencesWidget(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::TimeServerSelectionWidget),
    m_model(new QnTimeServerSelectionModel(this))
{
    ui->setupUi(this);
    ui->timeSourceLabel->setVisible(false);
    if (true) {
        ui->timeSourceLabel->setText(tr("Time is taken from the Internet."));
    } else {
        ui->timeSourceLabel->setText(tr("Time is taken from %1."));
    }

    QnSortServersByPriorityProxyModel* sortModel = new QnSortServersByPriorityProxyModel(this);
    sortModel->setSourceModel(m_model);

    ui->serversTable->setModel(sortModel);

    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::CheckboxColumn, QHeaderView::ResizeToContents);
    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::NameColumn, QHeaderView::Stretch);
    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::TimeColumn, QHeaderView::ResizeToContents);
    ui->serversTable->horizontalHeader()->setSectionResizeMode(QnTimeServerSelectionModel::OffsetColumn, QHeaderView::ResizeToContents);
    ui->serversTable->horizontalHeader()->setSectionsClickable(false);

    connect(qnSyncTime, &QnSyncTime::timeChanged, this, &QnTimeServerSelectionWidget::updateTime);

    QTimer* timer = new QTimer(this);
    timer->setInterval(1000);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this,  &QnTimeServerSelectionWidget::updateTime);
    timer->start();
}


QnTimeServerSelectionWidget::~QnTimeServerSelectionWidget() {
}


void QnTimeServerSelectionWidget::updateFromSettings() {
    PRINT_DEBUG("provide selected server to model:");
    m_model->setSelectedServer(selectedServer());
}

void QnTimeServerSelectionWidget::submitToSettings() {
    auto connection = QnAppServerConnectionFactory::getConnection2();
    if (!connection)
        return;

    PRINT_DEBUG("forcing selected server to " + m_model->selectedServer().toByteArray());
    auto timeManager = connection->getTimeManager();
    timeManager->forcePrimaryTimeServer(m_model->selectedServer(), this, [this](int handle, ec2::ErrorCode errCode){
        Q_UNUSED(handle);
        Q_UNUSED(errCode);  //suppress warning in the release code
        PRINT_DEBUG("forcing selected server finished with result " + ec2::toString(errCode).toUtf8());
    });

}

bool QnTimeServerSelectionWidget::hasChanges() const {
    PRINT_DEBUG("checking if the widget has changes " + m_model->selectedServer().toByteArray() + " vs...");
    return m_model->selectedServer() != selectedServer();
}

QnUuid QnTimeServerSelectionWidget::selectedServer() const {
    PRINT_DEBUG("check selected server by runtime info");

    foreach (const QnPeerRuntimeInfo &runtimeInfo, QnRuntimeInfoManager::instance()->items()->getItems()) {
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

void QnTimeServerSelectionWidget::updateTime() {
    if (!isVisible())
        return;

    m_model->updateTime();

    QDateTime syncTime;
    if (m_model->sameTimezone()) {
        syncTime.setTimeSpec(Qt::LocalTime);
        syncTime.setMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch());
        ui->timeLabel->setText(syncTime.toString(lit("yyyy-MM-dd HH:mm:ss")));
    } else {
        syncTime.setTimeSpec(Qt::UTC);
        syncTime.setMSecsSinceEpoch(qnSyncTime->currentMSecsSinceEpoch());
        ui->timeLabel->setText(syncTime.toString(lit("yyyy-MM-dd HH:mm:ss t")));
    }
}

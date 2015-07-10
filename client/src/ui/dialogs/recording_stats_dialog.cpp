#include "recording_stats_dialog.h"
#include "ui_recording_stats_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <core/resource_management/resource_pool.h>
#include <client/client_globals.h>
#include <client/client_settings.h>
#include <plugins/resource/server_camera/server_camera.h>
#include <ui/common/grid_widget_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include "ui/models/recording_stats_model.h"
#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>
#include <core/resource/media_server_resource.h>
#include "utils/common/event_processors.h"

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
}


QnRecordingStatsDialog::QnRecordingStatsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::RecordingStatsDialog),
    m_updateDisabled(false),
    m_dirty(false),
    m_lastMouseButton(Qt::NoButton)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    //setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);
    m_model = new QnRecordingStatsModel(this);

    QnSortedRecordingStatsModel* sortModel = new QnSortedRecordingStatsModel(this);
    sortModel->setSourceModel(m_model);
    ui->gridEvents->setModel(sortModel);

    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt.addYears(-1));
    ui->dateEditTo->setDate(dt);

    QHeaderView* headers = ui->gridEvents->horizontalHeader();
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridEvents->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer, &QnAbstractEventSignalizer::activated, this, &QnRecordingStatsDialog::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);

    ui->loadingProgressBar->hide();

    connect(m_clipboardAction,      &QAction::triggered,                this,   &QnRecordingStatsDialog::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                this,   &QnRecordingStatsDialog::at_exportAction_triggered);
    connect(m_selectAllAction,      &QAction::triggered,                ui->gridEvents, &QTableView::selectAll);

    connect(ui->dateEditFrom,       &QDateEdit::dateChanged,            this,   &QnRecordingStatsDialog::updateData);
    connect(ui->dateEditTo,         &QDateEdit::dateChanged,            this,   &QnRecordingStatsDialog::updateData);
    connect(ui->refreshButton,      &QAbstractButton::clicked,          this,   &QnRecordingStatsDialog::updateData);

    connect(ui->gridEvents,         &QTableView::clicked,               this,   &QnRecordingStatsDialog::at_eventsGrid_clicked);
    connect(ui->gridEvents,         &QTableView::customContextMenuRequested, this, &QnRecordingStatsDialog::at_eventsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE), &QnPropertyNotifier::valueChanged, ui->gridEvents, &QAbstractItemView::reset);

    ui->mainGridLayout->activate();
    updateHeaderWidth();
}

QnRecordingStatsDialog::~QnRecordingStatsDialog() {
}

void QnRecordingStatsDialog::updateData()
{
    if (m_updateDisabled) {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    query(ui->dateEditFrom->dateTime().toMSecsSinceEpoch(),
          ui->dateEditTo->dateTime().addDays(1).toMSecsSinceEpoch());

    // update UI

    if (!m_requests.isEmpty()) {
        ui->gridEvents->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
        ui->loadingProgressBar->show();
    }
    else {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    ui->dateEditFrom->setDateRange(QDate(2000,1,1), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), QDateTime::currentDateTime().date());

    m_updateDisabled = false;
    m_dirty = false;
}

QList<QnMediaServerResourcePtr> QnRecordingStatsDialog::getServerList() const
{
    QList<QnMediaServerResourcePtr> result;
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(const QnResourcePtr& r, resList) {
        QnMediaServerResourcePtr mServer = r.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

void QnRecordingStatsDialog::query(qint64 fromMsec, qint64 toMsec)
{
    m_requests.clear();

    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == Qn::Online)
        {
            m_requests << mserver->apiConnection()->getRecordingStatisticsAsync(
                fromMsec, toMsec,
                this, SLOT(at_gotStatiscits(int, const QnRecordingStatsReply&, int)));
        }
    }
}

void QnRecordingStatsDialog::updateHeaderWidth()
{
}

void QnRecordingStatsDialog::at_gotStatiscits(int httpStatus, const QnRecordingStatsReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0 && !data.isEmpty())
        m_allData << data;
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnRecordingStatsDialog::requestFinished()
{
    m_model->setModelData(m_allData);
    m_allData.clear();
    ui->gridEvents->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    updateHeaderWidth();
    if (ui->dateEditFrom->dateTime() != ui->dateEditTo->dateTime())
        ui->statusLabel->setText(tr("Recording statistics for period from %1 to %2 - %n camera(s) found", "", m_model->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(ui->dateEditTo->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    else
        ui->statusLabel->setText(tr("Recording statistics for %1 - %n camera(s) found", "", m_model->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    ui->loadingProgressBar->hide();
}

void QnRecordingStatsDialog::at_eventsGrid_clicked(const QModelIndex& idx)
{
}

void QnRecordingStatsDialog::setDateRange(const QDate& from, const QDate& to)
{
    ui->dateEditFrom->setDateRange(QDate(2000,1,1), to);
    ui->dateEditTo->setDateRange(from, QDateTime::currentDateTime().date());

    ui->dateEditTo->setDate(to);
    ui->dateEditFrom->setDate(from);
}

void QnRecordingStatsDialog::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QMenu* menu = 0;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = m_model->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();
        if (resource) {
            menu = manager->newMenu(Qn::TreeScope, this, QnActionParameters(resource));
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu(this);

    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnRecordingStatsDialog::at_exportAction_triggered()
{
    QnGridWidgetHelper::exportToFile(ui->gridEvents, this, tr("Export selected events to file"));
}

void QnRecordingStatsDialog::at_clipboardAction_triggered()
{
    QnGridWidgetHelper::copyToClipboard(ui->gridEvents);
}

void QnRecordingStatsDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void QnRecordingStatsDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void QnRecordingStatsDialog::enableUpdateData()
{
    m_updateDisabled = false;
    if (m_dirty) {
        m_dirty = false;
        if (isVisible())
            updateData();
    }
}

void QnRecordingStatsDialog::setVisible(bool value)
{
    // TODO: #Elric use showEvent instead. 

    if (value && !isVisible())
        updateData();
    QDialog::setVisible(value);
}

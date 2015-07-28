#include "audit_log_dialog.h"
#include "ui_audit_log_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>


#include <client/client_globals.h>
#include <client/client_settings.h>

#include <plugins/resource/server_camera/server_camera.h>
#include <ui/actions/action_manager.h>

#include <ui/common/grid_widget_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include "utils/common/event_processors.h"
#include <utils/common/scoped_painter_rollback.h>
#include "ui/actions/actions.h"
#include "utils/math/color_transformations.h"
#include "camera_addition_dialog.h"
#include <ui/models/audit/audit_log_session_model.h>
#include <ui/models/audit/audit_log_detail_model.h>

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
    const int BTN_ICON_SIZE = 16;
}

// --------------------------- QnAuditDetailItemDelegate ------------------------

QSize QnAuditDetailItemDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    QnAuditLogModel::Column column = (QnAuditLogModel::Column) index.data(Qn::ColumnDataRole).toInt();
    if (column == QnAuditLogModel::DescriptionColumn)
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord>())
            return base_type::sizeHint(option, index);
        QnAuditRecord record = data.value<QnAuditRecord>();

        QTextDocument txtDocument;
        txtDocument.setHtml(index.data(Qn::DisplayHtmlRole).toString());
        QSize size = txtDocument.size().toSize();
        bool showDetail = record.extractParam("detail") == "1";
        if (!showDetail)
            size.setHeight(m_defaultSectionSize);
        return size;
    }
    if (column == QnAuditLogModel::PlayButtonColumn) 
    {
        QFontMetrics fm(option.font);
        QSize sizeHint = fm.size(Qt::TextShowMnemonic, lit("Play this"));
        sizeHint += m_btnSize;
        sizeHint.setWidth(sizeHint.width() + BTN_ICON_SIZE);
        return sizeHint;
    }
    else 
        return base_type::sizeHint(option, index);

}

void QnAuditDetailItemDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    QnAuditLogModel::Column column = (QnAuditLogModel::Column) index.data(Qn::ColumnDataRole).toInt();

    if (column == QnAuditLogModel::DescriptionColumn) 
    {
        QTextDocument txtDocument;
        if (option.state & QStyle::State_MouseOver)
            txtDocument.setHtml(index.data(Qn::DisplayHtmlHoveredRole).toString());
        else
            txtDocument.setHtml(index.data(Qn::DisplayHtmlRole).toString());

        QnScopedPainterTransformRollback rollback(painter);
        QRect rect = option.rect;
        QPoint shift = rect.topLeft();
        painter->translate(shift);
        rect.translate(-shift);
        txtDocument.drawContents(painter, rect);
    }
    else if (column == QnAuditLogModel::PlayButtonColumn) 
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord>())
            return base_type::paint(painter, option, index);
        QnAuditRecord record = data.value<QnAuditRecord>();
        if (!record.isPlaybackType())
            return base_type::paint(painter, option, index);

        QStyleOptionButton button;
        button.text = lit("Play this");
        if (option.state & QStyle::State_MouseOver)
            button.icon = qnSkin->icon("slider/navigation/play_hovered.png");
        else
            button.icon = qnSkin->icon("slider/navigation/play.png");
        button.iconSize = QSize(BTN_ICON_SIZE, BTN_ICON_SIZE);
        button.state = option.state;

        QFontMetrics fm(option.font);
        QSize sizeHint = fm.size(Qt::TextShowMnemonic, button.text);
        sizeHint += m_btnSize;
        int dx = (option.rect.width() - sizeHint.width()) /2;
        button.rect.setTopLeft(option.rect.topLeft() + QPoint(dx, 4));
        button.rect.setSize(sizeHint);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
    }
    else {
        base_type::paint(painter, option, index);
    }
}

// ---------------------- QnAuditLogDialog ------------------------------

QnAuditRecordList QnAuditLogDialog::filteredChildData(const QnAuditRecordList& checkedRows)
{
    QSet<QnUuid> selectedSessions;
    for (const auto& record: checkedRows) 
        selectedSessions << record.sessionId;

    QnAuditRecordList result;
    auto filter = [&selectedSessions] (const QnAuditRecord& record) { 
        return selectedSessions.contains(record.sessionId); // && !record.isLoginType();
    };
    std::copy_if(m_allData.begin(), m_allData.end(), std::back_inserter(result), filter);
    return result;
}

QSize calcButtonSize(const QFont& font)
{
    static const QString TEST_TXT(lit("TEST"));
    QFontMetrics fm(font);
    QSize size1 = fm.size(Qt::TextShowMnemonic, TEST_TXT);

    std::unique_ptr<QPushButton> button(new QPushButton());
    button->setText(TEST_TXT);

    QSize size2 = button->minimumSizeHint();
    return QSize(size2.width() - size1.width(), size2.height() - size1.height());
}

QList<QnAuditLogModel::Column> detailSessionColumns(bool masterMultiselected)
{
    QList<QnAuditLogModel::Column> columns;
    columns << 
        QnAuditLogModel::DateColumn <<
        QnAuditLogModel::TimeColumn;
    if (masterMultiselected)
        columns << QnAuditLogModel::UserNameColumn << QnAuditLogModel::UserHostColumn;
    columns <<
        QnAuditLogModel::EventTypeColumn <<
        QnAuditLogModel::DescriptionColumn <<
        QnAuditLogModel::PlayButtonColumn;
    
    return columns;
}

QnAuditLogDialog::QnAuditLogDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::AuditLogDialog),
    m_updateDisabled(false),
    m_dirty(false)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    //setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);


    m_sessionModel = new QnAuditLogSessionModel(this);

    QList<QnAuditLogModel::Column> columns;
    columns << 
        QnAuditLogModel::SelectRowColumn <<
        QnAuditLogModel::TimestampColumn <<
        QnAuditLogModel::EndTimestampColumn <<
        QnAuditLogModel::DurationColumn <<
        QnAuditLogModel::UserNameColumn <<
        QnAuditLogModel::UserHostColumn;

    m_sessionModel->setColumns(columns);

    ui->gridMaster->setModel(m_sessionModel);
    ui->gridMaster->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    m_masterHeaders = new QnCheckBoxedHeaderView(this);
    connect (m_masterHeaders, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnAuditLogDialog::at_headerCheckStateChanged);
    ui->gridMaster->setHorizontalHeader(m_masterHeaders);
    m_masterHeaders->setVisible(true);
    m_masterHeaders->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_masterHeaders->setSectionsClickable(true);

    connect
        (
        m_sessionModel, &QAbstractItemModel::dataChanged, this,
        [this] (const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
    { 
        if (roles.contains(Qt::CheckStateRole)) {
            QnAuditRecordList checkedRows = m_sessionModel->checkedRows();
            m_detailModel->setData(filteredChildData(checkedRows));
            m_detailModel->setColumns(detailSessionColumns(checkedRows.size() > 1));
        }
    }
    );

    m_detailModel = new QnAuditLogDetailModel(this);

    m_detailModel->setColumns(detailSessionColumns(false));
    ui->gridDetails->setModel(m_detailModel);
    QnAuditDetailItemDelegate* delegate = new QnAuditDetailItemDelegate(this);
    delegate->setButtonExtraSize(calcButtonSize(ui->gridMaster->font()));
    delegate->setDefaultSectionHeight(ui->gridDetails->verticalHeader()->defaultSectionSize());
    ui->gridDetails->setItemDelegate(delegate);
    ui->gridDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->gridDetails->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    ui->gridDetails->setMouseTracking(true);


    connect(ui->gridMaster, &QTableView::pressed, this, &QnAuditLogDialog::at_masterItemPressed);
    connect(ui->gridDetails, &QTableView::pressed, this, &QnAuditLogDialog::at_ItemPressed);
    connect(ui->gridDetails, &QTableView::entered, this, &QnAuditLogDialog::at_ItemEntered);


    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt);
    ui->dateEditTo->setDate(dt);

    QHeaderView* headers = ui->gridMaster->horizontalHeader();
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);

    
    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridMaster->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer, &QnAbstractEventSignalizer::activated, this, &QnAuditLogDialog::at_mouseButtonRelease);

    ui->gridMaster->addAction(m_clipboardAction);
    ui->gridMaster->addAction(m_exportAction);

    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));
    ui->loadingProgressBar->hide();

    connect(m_clipboardAction,      &QAction::triggered,                this,   &QnAuditLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                this,   &QnAuditLogDialog::at_exportAction_triggered);
    connect(m_selectAllAction,      &QAction::triggered,                ui->gridMaster, &QTableView::selectAll);

    connect(ui->dateEditFrom,       &QDateEdit::dateChanged,            this,   &QnAuditLogDialog::updateData);
    connect(ui->dateEditTo,         &QDateEdit::dateChanged,            this,   &QnAuditLogDialog::updateData);
    connect(ui->refreshButton,      &QAbstractButton::clicked,          this,   &QnAuditLogDialog::updateData);

    connect(ui->gridMaster,         &QTableView::clicked,               this,   &QnAuditLogDialog::at_sessionsGrid_clicked);
    connect(ui->gridMaster,         &QTableView::customContextMenuRequested, this, &QnAuditLogDialog::at_sessionsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE), &QnPropertyNotifier::valueChanged, ui->gridMaster, &QAbstractItemView::reset);

    connect(ui->gridDetails,         &QTableView::clicked,               this,   &QnAuditLogDialog::at_eventsGrid_clicked);

    ui->mainGridLayout->activate();
    //updateHeaderWidth();
}

QnAuditLogDialog::~QnAuditLogDialog() {
}


void QnAuditLogDialog::at_eventsGrid_clicked(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::DescriptionColumn)
        return;

    QVariant data = index.data(Qn::AuditRecordDataRole);
    if (!data.canConvert<QnAuditRecord>())
        return;
    QnAuditRecord record = data.value<QnAuditRecord>();
    bool showDetail = record.extractParam("detail") == "1";
    showDetail = !showDetail;
    record.addParam("detail", showDetail ? "1" : "0");
    
    ui->gridDetails->model()->setData(index, QVariant::fromValue(record), Qn::AuditRecordDataRole);

    int height = ui->gridDetails->itemDelegate()->sizeHint(QStyleOptionViewItem(), index).height();
    ui->gridDetails->setRowHeight(index.row(), height);
}

void QnAuditLogDialog::at_ItemEntered(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) == QnAuditLogModel::DescriptionColumn)
        ui->gridDetails->setCursor(Qt::PointingHandCursor);
    else
        ui->gridDetails->setCursor(Qt::ArrowCursor);
}

void QnAuditLogDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    m_sessionModel->setCheckState(state);
}

void QnAuditLogDialog::at_masterItemPressed(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::SelectRowColumn)
        return;

    Qt::CheckState checkState = (Qt::CheckState) index.data(Qt::CheckStateRole).toInt();
    if (checkState == Qt::Checked)
        checkState = Qt::Unchecked;
    else
        checkState = Qt::Checked;
    ui->gridMaster->model()->setData(index, checkState, Qt::CheckStateRole);
    m_masterHeaders->setCheckState(m_sessionModel->checkState());
}

void QnAuditLogDialog::at_ItemPressed(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::PlayButtonColumn)
        return;

    QVariant data = index.data(Qn::AuditRecordDataRole);
    if (!data.canConvert<QnAuditRecord>())
        return;
    QnAuditRecord record = data.value<QnAuditRecord>();
    QnResourceList resList;
    QnByteArrayConstRef archiveData = record.extractParam("archiveExist");
    int i = 0;
    for (const auto& id: record.resources) {
        bool archiveExist = archiveData.size() > i && archiveData[i] == '1';
        i++;
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res && archiveExist)
            resList << res;
    }

    QnActionParameters params(resList);
    if (record.isPlaybackType()) 
    {
        if (resList.isEmpty()) {
            QMessageBox::information(this, tr("Information"), tr("No archive data for that position left"));
            return;
        }

        params.setArgument(Qn::ItemTimeRole, record.rangeStartSec * 1000ll);
        context()->menu()->trigger(Qn::OpenInNewLayoutAction, params);
    }
    if (isMaximized())
        showNormal();
}

void QnAuditLogDialog::updateData()
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
        ui->gridMaster->setDisabled(true);
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

QList<QnMediaServerResourcePtr> QnAuditLogDialog::getServerList() const
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

void QnAuditLogDialog::query(qint64 fromMsec, qint64 toMsec)
{
    m_requests.clear();
    m_allData.clear();


    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == Qn::Online)
        {
            m_requests << mserver->apiConnection()->getAuditLogAsync(
                fromMsec, toMsec,
                this, SLOT(at_gotdata(int, const QnAuditRecordList&, int)));
        }
    }
}

void QnAuditLogDialog::at_gotdata(int httpStatus, const QnAuditRecordList& records, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0)
        m_allData << records;
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnAuditLogDialog::requestFinished()
{
    auto sessionFilter = [] (const QnAuditRecord& record) { return record.isLoginType();  };
    QnAuditRecordList sessions;
    for (const auto& record: m_allData) {
        if (sessionFilter(record))
            sessions << record;
    }
    m_sessionModel->setData(sessions);
    m_masterHeaders->setCheckState(Qt::Unchecked);

    ui->gridMaster->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    //updateHeaderWidth();
    if (ui->dateEditFrom->dateTime() != ui->dateEditTo->dateTime())
        ui->statusLabel->setText(tr("Audit log for period from %1 to %2 - %n session(s) found", "", m_sessionModel->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(ui->dateEditTo->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    else
        ui->statusLabel->setText(tr("Audit log for %1 - %n session(s) found", "", m_sessionModel->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    ui->loadingProgressBar->hide();
}

void QnAuditLogDialog::at_sessionsGrid_clicked(const QModelIndex& idx)
{
}

void QnAuditLogDialog::setDateRange(const QDate& from, const QDate& to)
{
    ui->dateEditFrom->setDateRange(QDate(2000,1,1), to);
    ui->dateEditTo->setDateRange(from, QDateTime::currentDateTime().date());

    ui->dateEditTo->setDate(to);
    ui->dateEditFrom->setDate(from);
}

void QnAuditLogDialog::at_sessionsGrid_customContextMenuRequested(const QPoint&)
{
    QMenu* menu = 0;
    QModelIndex idx = ui->gridMaster->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = ui->gridMaster->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
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

    m_clipboardAction->setEnabled(ui->gridMaster->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridMaster->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnAuditLogDialog::at_exportAction_triggered()
{
    QnGridWidgetHelper::exportToFile(ui->gridMaster, this, tr("Export selected records to a file"));
}

void QnAuditLogDialog::at_clipboardAction_triggered()
{
    QnGridWidgetHelper::copyToClipboard(ui->gridMaster);
}

void QnAuditLogDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    //m_lastMouseButton = me->button();
}

void QnAuditLogDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void QnAuditLogDialog::enableUpdateData()
{
    m_updateDisabled = false;
    if (m_dirty) {
        m_dirty = false;
        if (isVisible())
            updateData();
    }
}

void QnAuditLogDialog::setVisible(bool value)
{
    // TODO: #Elric use showEvent instead. 

    if (value && !isVisible())
        updateData();
    QDialog::setVisible(value);
}

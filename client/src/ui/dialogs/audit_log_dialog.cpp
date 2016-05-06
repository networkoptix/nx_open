#include "audit_log_dialog.h"
#include "ui_audit_log_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>

#include <ui/common/grid_widget_helper.h>
#include <ui/delegates/audit_item_delegate.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/models/audit/audit_log_detail_model.h>
#include <ui/models/audit/audit_log_session_model.h>
#include <ui/style/custom_style.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>

#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/event_processors.h>

#include <ui/common/geometry.h>
#include <ui/common/palette.h>
#include <ui/style/globals.h>
#include <ui/style/helper.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/views/checkboxed_header_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>



namespace
{
    const int ProlongedActionRole = Qt::UserRole + 2;
    const int BTN_ICON_SIZE = 16;

    const int kRowHeightPixels = 24;

    enum MasterGridTabIndex
    {
        SessionTab,
        CameraTab
    };

    const char* checkBoxCheckedProperty("checkboxChecked");
    const char* checkBoxFilterProperty("checkboxFilter");
}

QnAuditLogDialog::QnAuditLogDialog(QWidget *parent) :
    base_type(parent),
    ui(new Ui::AuditLogDialog),
    m_updateDisabled(false),
    m_dirty(false)
{
    ui->setupUi(this);
    retranslateUi();

    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, Qn::AuditTrail_Help);

    setTabShape(ui->mainTabWidget->tabBar(), style::TabShape::Compact);
    setTabShape(ui->detailsTabWidget->tabBar(), style::TabShape::Compact);

    m_clipboardAction = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    m_itemDelegate = new QnAuditItemDelegate(this);

    setupSessionsGrid();
    setupCamerasGrid();
    setupDetailsGrid();

    at_updateCheckboxes();

    connect(m_sessionModel, &QnAuditLogModel::colorsChanged, this, &QnAuditLogDialog::at_updateCheckboxes);
    connect(ui->selectAllCheckBox, &QCheckBox::stateChanged, this, &QnAuditLogDialog::at_selectAllCheckboxChanged);

    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt);
    ui->dateEditTo->setDate(dt);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);

    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));
    ui->loadingProgressBar->hide();

    connect(ui->mainTabWidget, &QTabWidget::currentChanged, this, &QnAuditLogDialog::at_currentTabChanged);

    connect(m_clipboardAction,  &QAction::triggered,        this, &QnAuditLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction,     &QAction::triggered,        this, &QnAuditLogDialog::at_exportAction_triggered);

    connect(ui->dateEditFrom,   &QDateEdit::dateChanged,    this, &QnAuditLogDialog::updateData);
    connect(ui->dateEditTo,     &QDateEdit::dateChanged,    this, &QnAuditLogDialog::updateData);
    connect(ui->refreshButton,  &QAbstractButton::clicked,  this, &QnAuditLogDialog::updateData);
    connect(ui->filterLineEdit, &QLineEdit::textChanged,    this, &QnAuditLogDialog::at_filterChanged);

    ui->mainGridLayout->activate();

    ui->filterLineEdit->setPlaceholderText(tr("Search"));

    ui->gridMaster->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    ui->gridCameras->horizontalHeader()->setSortIndicator(1, Qt::AscendingOrder);
}

QnAuditLogDialog::~QnAuditLogDialog()
{
}

void QnAuditLogDialog::setupSessionsGrid()
{
    QList<QnAuditLogModel::Column> columns;
    columns <<
        QnAuditLogModel::SelectRowColumn <<
        QnAuditLogModel::TimestampColumn <<
        QnAuditLogModel::EndTimestampColumn <<
        QnAuditLogModel::DurationColumn <<
        QnAuditLogModel::UserNameColumn <<
        QnAuditLogModel::UserHostColumn <<
        QnAuditLogModel::UserActivityColumn;

    m_sessionModel = new QnAuditLogMasterModel(this);
    m_sessionModel->setColumns(columns);
    ui->gridMaster->setCheckboxColumn(columns.indexOf(QnAuditLogModel::SelectRowColumn));
    ui->gridMaster->setWholeModelChange(true);
    ui->gridMaster->setModel(m_sessionModel);

    setupGridCommon(ui->gridMaster, true);
}

void QnAuditLogDialog::setupCamerasGrid()
{
    QList<QnAuditLogModel::Column> columns;
    columns <<
        QnAuditLogModel::SelectRowColumn <<
        QnAuditLogModel::CameraNameColumn <<
        QnAuditLogModel::CameraIpColumn <<
        QnAuditLogModel::UserActivityColumn;

    m_camerasModel = new QnAuditLogMasterModel(this);
    m_camerasModel->setColumns(columns);
    ui->gridCameras->setCheckboxColumn(columns.indexOf(QnAuditLogModel::SelectRowColumn));
    ui->gridCameras->setWholeModelChange(true);
    ui->gridCameras->setModel(m_camerasModel);

    setupGridCommon(ui->gridCameras, true);

    ui->gridCameras->horizontalHeader()->setSectionResizeMode(columns.indexOf(QnAuditLogModel::CameraNameColumn), QHeaderView::Stretch);
    ui->gridCameras->horizontalHeader()->setSectionResizeMode(columns.indexOf(QnAuditLogModel::UserActivityColumn), QHeaderView::Stretch);
}

void QnAuditLogDialog::setupDetailsGrid()
{
    QList<QnAuditLogModel::Column> columns;
    columns
        << QnAuditLogModel::DateColumn
        << QnAuditLogModel::TimeColumn
        << QnAuditLogModel::UserNameColumn
        << QnAuditLogModel::UserHostColumn
        << QnAuditLogModel::EventTypeColumn
        << QnAuditLogModel::DescriptionColumn;

    m_detailModel = new QnAuditLogDetailModel(this);
    m_detailModel->setColumns(columns);
    ui->gridDetails->setModel(m_detailModel);
    ui->gridDetails->setSelectionMode(QAbstractItemView::NoSelection);
    ui->gridDetails->setWordWrap(true);

    setupGridCommon(ui->gridDetails, false);

    ui->gridDetails->horizontalHeader()->setMinimumSectionSize(48);
    ui->gridDetails->horizontalHeader()->setSectionResizeMode(columns.indexOf(QnAuditLogModel::DescriptionColumn), QHeaderView::Stretch);

    connect(m_itemDelegate, &QnAuditItemDelegate::buttonClicked,        this, &QnAuditLogDialog::at_itemButtonClicked);
    connect(m_itemDelegate, &QnAuditItemDelegate::descriptionPressed,   this, &QnAuditLogDialog::at_descriptionPressed);
}

void QnAuditLogDialog::setupGridCommon(QnTableView* grid, bool master)
{
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(grid->parentWidget());
    grid->setVerticalScrollBar(scrollBar->proxyScrollBar());

    QFont gridFont(font());
    gridFont.setPixelSize(gridFont.pixelSize() - 2);
    grid->setFont(gridFont);

    if (master)
    {
        QnCheckBoxedHeaderView* header = new QnCheckBoxedHeaderView(QnAuditLogModel::SelectRowColumn, this);
        header->setVisible(true);
        header->setSectionsClickable(true);
        grid->setHorizontalHeader(header);
        connect(header, &QnCheckBoxedHeaderView::checkStateChanged, this, &QnAuditLogDialog::at_headerCheckStateChanged);
    }

    grid->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    grid->verticalHeader()->setMinimumSectionSize(style::Metrics::kViewRowHeight);
    grid->verticalHeader()->setDefaultSectionSize(style::Metrics::kViewRowHeight);

    grid->setItemDelegate(m_itemDelegate);
    grid->setMouseTracking(true);

    if (master)
    {
        connect(grid->model(), &QAbstractItemModel::dataChanged, this, &QnAuditLogDialog::at_updateDetailModel);
        connect(grid->model(), &QAbstractItemModel::modelReset,  this, &QnAuditLogDialog::at_updateDetailModel);
        connect(grid->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QnAuditLogDialog::at_masterGridSelectionChanged);
    }

    setupContextMenu(grid);
}

void QnAuditLogDialog::setupContextMenu(QTableView* gridMaster)
{
    gridMaster->addAction(m_clipboardAction);
    gridMaster->addAction(m_exportAction);
    connect(gridMaster, &QTableView::customContextMenuRequested, this, &QnAuditLogDialog::at_customContextMenuRequested);
    connect(m_selectAllAction, &QAction::triggered, this, &QnAuditLogDialog::at_selectAllAction_triggered);
}

QnAuditRecordRefList QnAuditLogDialog::applyFilter()
{
    QnAuditRecordRefList result;
    QStringList keywords = ui->filterLineEdit->text().split(lit(" "));

    Qn::AuditRecordTypes disabledTypes = Qn::AR_NotDefined;
    for (const QCheckBox* checkBox: m_filterCheckboxes)
    {
        if (!checkBox->isChecked())
            disabledTypes |= (Qn::AuditRecordTypes) checkBox->property(checkBoxFilterProperty).toInt();
    }

    auto filter = [&keywords, &disabledTypes, this] (const QnAuditRecord& record)
    {
        if (disabledTypes & record.eventType)
            return false;

        bool matched = true;
        QString wholeText = m_camerasModel->makeSearchPattern(&record);
        for (const auto& keyword: keywords)
        {
            if (!wholeText.contains(keyword, Qt::CaseInsensitive))
            {
                matched = false;
                break;
            }
        }
        return matched;
    };
    for (QnAuditRecord& record: m_allData)
    {
        if (filter(record))
            result.push_back(&record);
    }
    return result;
}


QnAuditRecordRefList QnAuditLogDialog::filterChildDataBySessions(const QnAuditRecordRefList& checkedRows)
{
    QMap<QnUuid, int> selectedSessions;
    for (const QnAuditRecord* record: checkedRows)
        selectedSessions.insert(record->authSession.id, record->rangeStartSec);

    QnAuditRecordRefList result;
    auto filter = [&selectedSessions] (const QnAuditRecord* record)
    {
        if (record->eventType == Qn::AR_Login)
            return selectedSessions.value(record->authSession.id) == record->rangeStartSec; // hide duplicate login from difference servers
        else
            return selectedSessions.contains(record->authSession.id);
    };
    std::copy_if(m_filteredData.begin(), m_filteredData.end(), std::back_inserter(result), filter);
    return result;
}

QnAuditRecordRefList QnAuditLogDialog::filterChildDataByCameras(const QnAuditRecordRefList& checkedRows)
{
    QSet<QnUuid> selectedCameras;
    for (const QnAuditRecord* record: checkedRows)
        selectedCameras << record->resources[0];

    QnAuditRecordRefList result;
    auto filter = [&selectedCameras] (const QnAuditRecord* record)
    {
        for (const auto& id: record->resources)
        {
            if (selectedCameras.contains(id))
                return true;
        }
        return false;
    };
    std::copy_if(m_filteredData.begin(), m_filteredData.end(), std::back_inserter(result), filter);
    return result;
}

void QnAuditLogDialog::setupFilterCheckbox(QCheckBox* checkbox, const QColor& color, Qn::AuditRecordTypes filteredTypes)
{
    setPaletteColor(checkbox, QPalette::Active, QPalette::Text, color);
    setPaletteColor(checkbox, QPalette::Inactive, QPalette::Text, color);
    checkbox->setProperty(checkBoxFilterProperty, static_cast<int>(filteredTypes));
    m_filterCheckboxes << checkbox;
    checkbox->disconnect(this);
    connect(checkbox, &QCheckBox::stateChanged, this, &QnAuditLogDialog::at_typeCheckboxChanged);
}

void QnAuditLogDialog::at_typeCheckboxChanged()
{
    bool hasChecked = false;
    bool hasUnchecked = false;
    for (const auto& checkbox: m_filterCheckboxes)
    {
        if (!checkbox->isEnabled())
            continue;

        if (checkbox->isChecked())
            hasChecked = true;
        else
            hasUnchecked = true;
    }

    Qt::CheckState checkState = Qt::Unchecked;
    if (hasChecked && hasUnchecked)
        checkState = Qt::PartiallyChecked;
    else if(hasChecked)
        checkState = Qt::Checked;

    //TODO: #GDM get rid of this magic and use common setupTristateCheckbox framework
    ui->selectAllCheckBox->blockSignals(true);
    ui->selectAllCheckBox->setCheckState(checkState);
    ui->selectAllCheckBox->blockSignals(false);

    at_filterChanged();
};

void QnAuditLogDialog::at_selectAllCheckboxChanged()
{
    Qt::CheckState state = ui->selectAllCheckBox->checkState();
    if (state == Qt::PartiallyChecked)
    {
        ui->selectAllCheckBox->setChecked(true);
        return;
    }

    for (const auto& checkbox: m_filterCheckboxes)
    {
        if (!checkbox->isEnabled())
            continue;
        checkbox->blockSignals(true);
        checkbox->setChecked(state == Qt::Checked);
        checkbox->blockSignals(false);
    }
    at_filterChanged();
};

void QnAuditLogDialog::at_currentTabChanged()
{
    bool allEnabled = (ui->mainTabWidget->currentIndex() == SessionTab);
    const Qn::AuditRecordTypes camerasTypes = Qn::AR_ViewLive | Qn::AR_ViewArchive | Qn::AR_ExportVideo | Qn::AR_CameraUpdate | Qn::AR_CameraInsert | Qn::AR_CameraRemove;

    for(QCheckBox* checkBox: m_filterCheckboxes)
    {
        Qn::AuditRecordTypes eventTypes =static_cast<Qn::AuditRecordTypes>(checkBox->property(checkBoxFilterProperty).toInt());
        bool allowed = allEnabled || (camerasTypes & eventTypes);

        if (checkBox->isEnabled() == allowed)
            continue;

        checkBox->setEnabled(allowed);
        if (allowed)
        {
            checkBox->setChecked(checkBox->property(checkBoxCheckedProperty).toBool());
        }
        else
        {
            checkBox->setProperty(checkBoxCheckedProperty, checkBox->isChecked());
            checkBox->setChecked(false);
        }
    }
    at_filterChanged();
}

void QnAuditLogDialog::at_filterChanged()
{
    m_filteredData = applyFilter();

    if (ui->mainTabWidget->currentIndex() == SessionTab)
    {
        QSet<QnUuid> filteredIdList;
        for (const QnAuditRecord* record: m_filteredData)
            filteredIdList << record->authSession.id;

        QnAuditRecordRefList filteredSessions;
        for (auto& record: m_sessionData)
        {
            if (filteredIdList.contains(record.authSession.id))
                filteredSessions.push_back(&record);
        }

        m_sessionModel->setData(filteredSessions);
    }
    else
    {
        QSet<QnUuid> filteredCameras;
        for (const QnAuditRecord* record: m_filteredData)
        {
            for (const auto& id: record->resources)
            filteredCameras << id;
        }

        QnAuditRecordRefList cameras;
        for (auto& record: m_cameraData)
        {
            if (filteredCameras.contains(record.resources[0]))
                cameras.push_back(&record);
        }
        m_camerasModel->setData(cameras);
    }
}

void QnAuditLogDialog::at_updateDetailModel()
{
    if (ui->mainTabWidget->currentIndex() == SessionTab)
    {
        QnAuditRecordRefList checkedRows = m_sessionModel->checkedRows();
        auto data = filterChildDataBySessions(checkedRows);
        m_detailModel->setData(data);
        m_detailModel->calcColorInterleaving();
    }
    else
    {
        QnAuditRecordRefList checkedRows = m_camerasModel->checkedRows();
        auto data = filterChildDataByCameras(checkedRows);
        m_detailModel->setData(data);
    }

    /// TODO: #ynikitenkov introduce expanded-state management in model

    /// Collapses all expanded descriptions (cameras, for examples)
    for (int row = 0; row != m_detailModel->rowCount(); ++row)
    {
        const QModelIndex index = m_detailModel->QnAuditLogModel::index(row, 0);
        const QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord *>())
            continue;

        QnAuditRecord * const record = data.value<QnAuditRecord *>();
        m_detailModel->setDetail(record, false);
    }
}

void QnAuditLogDialog::at_updateCheckboxes()
{
    m_filterCheckboxes.clear();
    QnAuditLogColors colors = m_sessionModel->colors();
    setupFilterCheckbox(ui->checkBoxLogin,      colors.loginAction,     Qn::AR_UnauthorizedLogin | Qn::AR_Login);
    setupFilterCheckbox(ui->checkBoxUsers,      colors.updUsers,        Qn::AR_UserUpdate | Qn::AR_UserRemove);
    setupFilterCheckbox(ui->checkBoxLive,       colors.watchingLive,    Qn::AR_ViewLive);
    setupFilterCheckbox(ui->checkBoxArchive,    colors.watchingArchive, Qn::AR_ViewArchive);
    setupFilterCheckbox(ui->checkBoxExport,     colors.exportVideo,     Qn::AR_ExportVideo);
    setupFilterCheckbox(ui->checkBoxCameras,    colors.updCamera,       Qn::AR_CameraUpdate | Qn::AR_CameraRemove | Qn::AR_CameraInsert);
    setupFilterCheckbox(ui->checkBoxSystem,     colors.systemActions,   Qn::AR_SystemNameChanged | Qn::AR_SystemmMerge | Qn::AR_SettingsChange | Qn::AR_DatabaseRestore);
    setupFilterCheckbox(ui->checkBoxServers,    colors.updServer,       Qn::AR_ServerUpdate | Qn::AR_ServerRemove);
    setupFilterCheckbox(ui->checkBoxBRules,     colors.eventRules,      Qn::AR_BEventUpdate | Qn::AR_BEventRemove | Qn::AR_BEventReset);
    setupFilterCheckbox(ui->checkBoxEmail,      colors.emailSettings,   Qn::AR_EmailSettings);
};

void setGridGeneralCheckState(QTableView* gridMaster)
{
    if (auto header = qobject_cast<QnCheckBoxedHeaderView*>(gridMaster->horizontalHeader()))
    {
        QSignalBlocker blocker(header);
        int numSelectedRows = gridMaster->selectionModel()->selectedRows().size();

        header->setCheckState((numSelectedRows == 0) ? Qt::Unchecked :
            ((numSelectedRows < gridMaster->model()->rowCount()) ? Qt::PartiallyChecked : Qt::Checked));
    }
}

void QnAuditLogDialog::at_masterGridSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QN_UNUSED(selected, deselected);
    if (auto tableView = qobject_cast<QTableView*>(sender()->parent()))
        setGridGeneralCheckState(tableView);
}

void QnAuditLogDialog::at_descriptionPressed(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::DescriptionColumn)
        return;

    QVariant data = index.data(Qn::AuditRecordDataRole);
    if (!data.canConvert<QnAuditRecord*>())
        return;

    QnAuditRecord* record = data.value<QnAuditRecord*>();
    bool showDetail = QnAuditLogModel::hasDetail(record);
    showDetail = !showDetail;
    QnAuditLogModel::setDetail(record, showDetail);

    int height = ui->gridDetails->itemDelegate()->sizeHint(QStyleOptionViewItem(), index).height();
    ui->gridDetails->setRowHeight(index.row(), height);
}

void QnAuditLogDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    QTableView* masterGrid = static_cast<QTableView*>(sender()->parent());
    switch (state)
    {
    case Qt::Checked:
        masterGrid->selectAll();
        break;
    case Qt::Unchecked:
        masterGrid->clearSelection();
        break;
    }
}

void QnAuditLogDialog::processPlaybackAction(const QnAuditRecord* record)
{
    QnResourceList resList;
    QnByteArrayConstRef archiveData = record->extractParam("archiveExist");
    int i = 0;
    for (const auto& id: record->resources)
    {
        bool archiveExist = archiveData.size() > i && archiveData[i] == '1';
        i++;
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res && archiveExist)
            resList << res;
    }

    QnActionParameters params(resList);
    if (resList.isEmpty())
    {
        QnMessageBox::information(this, tr("Information"), tr("No archive data for that position left"));
        return;
    }

    QnTimePeriod period;
    period.startTimeMs = record->rangeStartSec * 1000ll;
    period.durationMs = (record->rangeEndSec - record->rangeStartSec) * 1000ll;

    params.setArgument(Qn::ItemTimeRole, period.startTimeMs);
    if (period.durationMs > 0)
        params.setArgument(Qn::TimePeriodRole, period);

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnLayoutResource());
    layout->addFlags(Qn::local);
    qnResPool->markLayoutAutoGenerated(layout);
    layout->setId(QnUuid::createUuid());
    layout->setName(tr("Audit log replay"));
    if(context()->user())
        layout->setParentId(context()->user()->getId());

    /* Calculate size of the resulting matrix. */
    qreal desiredItemAspectRatio = qnGlobals->defaultLayoutCellAspectRatio();

    /* Calculate best size for layout cells. */
    qreal desiredCellAspectRatio = desiredItemAspectRatio;

    /* Aspect ratio of the screen free space. */
    QRectF viewportGeometry = display()->boundedViewportGeometry();

    qreal displayAspectRatio = viewportGeometry.isNull()
        ? desiredItemAspectRatio
        : QnGeometry::aspectRatio(viewportGeometry);

    if (resList.size() == 1)
    {
        if (QnMediaResourcePtr mediaRes = resList[0].dynamicCast<QnMediaResource>())
        {
            qreal customAspectRatio = mediaRes->customAspectRatio();
            if (!qFuzzyIsNull(customAspectRatio))
                desiredCellAspectRatio = customAspectRatio;
        }
    }

    const int matrixWidth = qMax(1, qRound(std::sqrt(displayAspectRatio * resList.size() / desiredCellAspectRatio)));

    for(int i = 0; i < resList.size(); i++)
    {
        QnLayoutItemData item;
        item.uuid = QnUuid::createUuid();
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        item.resource.id = resList[i]->getId();
        item.resource.path = resList[i]->getUniqueId();
        item.dataByRole[Qn::ItemTimeRole] = period.startTimeMs;

        QString forcedRotation = resList[i]->getProperty(QnMediaResource::rotationKey());
        if (!forcedRotation.isEmpty())
            item.rotation = forcedRotation.toInt();

        layout->addItem(item);
    }

    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredCellAspectRatio);
    layout->setCellAspectRatio(desiredCellAspectRatio);
    layout->setLocalRange(period);

    resourcePool()->addResource(layout);
    menu()->trigger(QnActions::OpenSingleLayoutAction, layout);

}

void QnAuditLogDialog::triggerAction(const QnAuditRecord* record, QnActions::IDType ActionId)
{
    QnResourceList resList;
    for (const auto& id: record->resources)
    {
        if (QnResourcePtr res = qnResPool->getResourceById(id))
            resList << res;
    }

    QnActionParameters params(resList);
    if (resList.isEmpty())
    {
        QnMessageBox::information(this, tr("Information"), tr("This resources are already removed from the system"));
        return;
    }

    params.setArgument(Qn::ItemTimeRole, record->rangeStartSec * 1000ll);
    context()->menu()->trigger(ActionId, params);
}

void QnAuditLogDialog::at_itemButtonClicked(const QModelIndex& index)
{
    NX_ASSERT(index.data(Qn::ColumnDataRole) == QnAuditLogModel::DescriptionColumn);

    QVariant data = index.data(Qn::AuditRecordDataRole);
    if (!data.canConvert<QnAuditRecord*>())
        return;

    QnAuditRecord* record = data.value<QnAuditRecord*>();
    if (record->isPlaybackType())
        processPlaybackAction(record);
    else if (record->eventType == Qn::AR_UserUpdate)
        triggerAction(record, QnActions::UserSettingsAction);
    else if (record->eventType == Qn::AR_ServerUpdate)
        triggerAction(record, QnActions::ServerSettingsAction);
    else if (record->eventType == Qn::AR_CameraUpdate || record->eventType == Qn::AR_CameraInsert)
        triggerAction(record, QnActions::CameraSettingsAction);

    if (isMaximized())
        showNormal();
}

void QnAuditLogDialog::updateData()
{
    if (m_updateDisabled)
    {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    query(ui->dateEditFrom->dateTime().toMSecsSinceEpoch(),
          ui->dateEditTo->dateTime().addDays(1).toMSecsSinceEpoch());

    // update UI

    if (!m_requests.isEmpty())
    {
        ui->gridMaster->setDisabled(true);
        ui->gridCameras->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
        ui->loadingProgressBar->show();
    }
    else
    {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    ui->dateEditFrom->setDateRange(QDate(2000,1,1), ui->dateEditTo->date());
    ui->dateEditTo->setDateRange(ui->dateEditFrom->date(), QDateTime::currentDateTime().date().addMonths(1)); // 1 month forward should cover all local timezones diffs.

    m_updateDisabled = false;
    m_dirty = false;
}

void QnAuditLogDialog::query(qint64 fromMsec, qint64 toMsec)
{
    m_sessionModel->clearData();
    m_camerasModel->clearData();
    m_detailModel->clearData();
    m_requests.clear();

    m_allData.clear();
    m_sessionData.clear();
    m_cameraData.clear();
    m_filteredData.clear();


    const auto onlineServers = qnResPool->getAllServers(Qn::Online);
    for(const QnMediaServerResourcePtr& mserver: onlineServers)
    {
        m_requests << mserver->apiConnection()->getAuditLogAsync(
            fromMsec, toMsec, this, SLOT(at_gotdata(int, const QnAuditRecordList&, int)));
    }
}

void QnAuditLogDialog::at_gotdata(int httpStatus, const QnAuditRecordList& records, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0)
        m_allData << records;
    if (m_requests.isEmpty())
        requestFinished();
}

void QnAuditLogDialog::makeSessionData()
{
    m_sessionData.clear();
    QMap<QnUuid, int> activityPerSession;
    QMap<QnUuid, QnAuditRecord> processedLogins;
    for (const QnAuditRecord& record: m_allData)
    {
        if (record.isLoginType())
        {
            auto itr = processedLogins.find(record.authSession.id);
            if (itr == processedLogins.end())
            {
                processedLogins.insert(record.authSession.id, record);
            }
            else
            {
                // group sessions because of different servers may have same session
                QnAuditRecord& existRecord = itr.value();
                existRecord.rangeStartSec = qMin(existRecord.rangeStartSec, record.rangeStartSec);
                existRecord.rangeEndSec = qMax(existRecord.rangeEndSec, record.rangeEndSec);
            }
        }
        activityPerSession[record.authSession.id]++;
    }
    m_sessionData.reserve(processedLogins.size());
    for (auto& value: processedLogins)
        m_sessionData.push_back(std::move(value));
    for (QnAuditRecord& record: m_sessionData) {
        record.addParam(QnAuditLogModel::ChildCntParamName, QByteArray::number(activityPerSession.value(record.authSession.id)));
        record.addParam(QnAuditLogModel::CheckedParamName, "1");
    }
}

void QnAuditLogDialog::makeCameraData()
{
    m_cameraData.clear();
    QMap<QnUuid, int> activityPerCamera;
    for (const QnAuditRecord& record: m_allData)
    {
        if (record.isPlaybackType() || record.isCameraType())
        {
            for (const QnUuid& cameraId: record.resources)
                activityPerCamera[cameraId]++;
        }
    }
    for (auto itr = activityPerCamera.begin(); itr != activityPerCamera.end(); ++itr)
    {
        QnAuditRecord cameraRecord;
        cameraRecord.resources.push_back(itr.key());
        cameraRecord.addParam(QnAuditLogModel::ChildCntParamName, QByteArray::number(itr.value())); // used for "user activity" column
        cameraRecord.addParam(QnAuditLogModel::CheckedParamName, "1");
        m_cameraData.push_back(cameraRecord);
    }
}

void QnAuditLogDialog::requestFinished()
{
    makeSessionData();
    makeCameraData();

    at_filterChanged();

    ui->gridMaster->setDisabled(false);
    ui->gridCameras->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    /*
    if (ui->dateEditFrom->dateTime() != ui->dateEditTo->dateTime())
        ui->statusLabel->setText(tr("Audit trail for period from %1 to %2 - %n session(s) found", "", m_sessionModel->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate))
        .arg(ui->dateEditTo->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    else
        ui->statusLabel->setText(tr("Audit trail for %1 - %n session(s) found", "", m_sessionModel->rowCount())
        .arg(ui->dateEditFrom->dateTime().date().toString(Qt::SystemLocaleLongDate)));
    */
    ui->loadingProgressBar->hide();
}

void QnAuditLogDialog::setDateRange(const QDate& from, const QDate& to)
{
    ui->dateEditFrom->setDateRange(QDate(2000,1,1), to);
    ui->dateEditTo->setDateRange(from, QDateTime::currentDateTime().date());

    ui->dateEditTo->setDate(to);
    ui->dateEditFrom->setDate(from);
}

void QnAuditLogDialog::at_customContextMenuRequested(const QPoint&)
{
    QMenu* menu = 0;
    QTableView* gridMaster = (QTableView*) sender();
    QModelIndex idx = gridMaster->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = gridMaster->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();

        if (resource)
        {
            QnActionParameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

            menu = manager->newMenu(Qn::TreeScope, this, parameters);
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu(this);

    m_clipboardAction->setEnabled(gridMaster->selectionModel()->hasSelection());
    m_exportAction->setEnabled(gridMaster->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

QTableView* QnAuditLogDialog::currentGridView() const
{
    if (ui->gridDetails->hasFocus())
        return ui->gridDetails;
    else if (ui->mainTabWidget->currentIndex() == SessionTab)
        return ui->gridMaster;
    else
        return ui->gridCameras;
}

void QnAuditLogDialog::retranslateUi()
{
    ui->retranslateUi(this);

    enum { kDevicesTabIndex = 1 };
    ui->mainTabWidget->setTabText(kDevicesTabIndex, QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Devices"),
        tr("Cameras")
    ));
    ui->checkBoxCameras->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        tr("Device actions"),
        tr("Camera actions")
    ));
}

void QnAuditLogDialog::at_exportAction_triggered()
{
    if (currentGridView())
        QnGridWidgetHelper::exportToFile(currentGridView(), this, tr("Export selected records to a file"));
}

void QnAuditLogDialog::at_selectAllAction_triggered()
{
    if (currentGridView())
        currentGridView()->selectAll();
}

void QnAuditLogDialog::at_clipboardAction_triggered()
{
    if (currentGridView())
        QnGridWidgetHelper::copyToClipboard(currentGridView());
}

void QnAuditLogDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void QnAuditLogDialog::enableUpdateData()
{
    m_updateDisabled = false;
    if (m_dirty)
    {
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

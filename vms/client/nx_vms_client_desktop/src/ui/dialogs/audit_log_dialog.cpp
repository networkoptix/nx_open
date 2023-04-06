// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_log_dialog.h"
#include "ui_audit_log_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMenu>

#include <api/server_rest_connection.h>
#include <client/client_globals.h>
#include <client/client_settings.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/item_view_hover_tracker.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/item_view_auto_hider.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <ui/common/palette.h>
#include <ui/delegates/audit_item_delegate.h>
#include <ui/dialogs/resource_properties/server_settings_dialog.h>
#include <ui/dialogs/resource_properties/user_settings_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/audit/audit_log_detail_model.h>
#include <ui/models/audit/audit_log_session_model.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>

using namespace nx::vms::client::desktop;
using namespace ui;

namespace {

enum MasterGridTabIndex
{
    SessionTab,
    CameraTab
};

const char* checkBoxCheckedProperty("checkboxChecked");
const char* checkBoxFilterProperty("checkboxFilter");

static constexpr qreal kDisabledCheckboxOpacity = 0.5;

} // namespace

QnAuditLogDialog::QnAuditLogDialog(QWidget* parent) :
    base_type(parent),
    ui(new Ui::AuditLogDialog),
    m_sessionModel(nullptr),
    m_camerasModel(nullptr),
    m_detailModel(nullptr),
    m_updateDisabled(false),
    m_dirty(false),
    m_selectAllAction(new QAction(tr("Select All"), this)),
    m_exportAction(new QAction(tr("Export Selection to File..."), this)),
    m_clipboardAction(new QAction(tr("Copy Selection to Clipboard"), this)),
    m_detailsLabel(nullptr),
    m_itemDelegate(new QnAuditItemDelegate(this)),
    m_descriptionColumnIndex(-1)
{
    ui->setupUi(this);
    retranslateUi();

    setWindowFlags(windowFlags()
        | Qt::WindowMaximizeButtonHint
        | Qt::MaximizeUsingFullscreenGeometryHint);

    setWarningStyle(ui->warningLabel);

    setHelpTopic(this, Qn::AuditTrail_Help);

    setTabShape(ui->mainTabWidget->tabBar(), nx::style::TabShape::Compact);
    setTabShape(ui->detailsTabWidget->tabBar(), nx::style::TabShape::Compact);

    autoResizePagesToContents(ui->mainTabWidget,
        { QSizePolicy::Fixed, QSizePolicy::Preferred }, true);

    /* Setup details label and its aligning by detailsTabWidget: */
    m_detailsLabel = new QLabel(ui->detailsTabWidget);
    installEventHandler({ ui->detailsTabWidget, m_detailsLabel }, QEvent::Resize, this,
        [this]()
        {
            m_detailsLabel->move(
                ui->detailsTabWidget->width() - m_detailsLabel->width(),
                (ui->detailsTabWidget->tabBar()->height() - m_detailsLabel->height()) / 2);
        });

    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    setupSessionsGrid();
    setupCamerasGrid();
    setupDetailsGrid();
    reset();

    setupCheckboxes();

    connect(ui->selectAllCheckBox, &QCheckBox::stateChanged,    this, &QnAuditLogDialog::at_selectAllCheckboxChanged);

    connect(ui->dateRangeWidget, &QnDateRangeWidget::rangeChanged, this, &QnAuditLogDialog::updateData);

    ui->refreshButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    ui->clearFilterButton->setIcon(qnSkin->icon("text_buttons/clear.png"));

    connect(ui->mainTabWidget,  &QTabWidget::currentChanged,    this, &QnAuditLogDialog::at_currentTabChanged);

    connect(m_clipboardAction,  &QAction::triggered,            this, &QnAuditLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction,     &QAction::triggered,            this, &QnAuditLogDialog::at_exportAction_triggered);

    connect(ui->refreshButton,  &QAbstractButton::clicked,      this, &QnAuditLogDialog::updateData);
    connect(ui->clearFilterButton, &QPushButton::clicked, this,
        &QnAuditLogDialog::reset);

    enum { kUpdateFilterDelayMs = 200 };
    ui->filterLineEdit->setTextChangedSignalFilterMs(kUpdateFilterDelayMs);

    connect(ui->filterLineEdit, &SearchLineEdit::enterKeyPressed, this, &QnAuditLogDialog::at_filterChanged);
    connect(ui->filterLineEdit, &SearchLineEdit::textChanged, this, &QnAuditLogDialog::at_filterChanged);

    ui->gridMaster->horizontalHeader()->setSortIndicator(1, Qt::DescendingOrder);
    ui->gridCameras->horizontalHeader()->setSortIndicator(1, Qt::AscendingOrder);

    /* Cursor changes when description is hovered: */
    connect(ui->gridDetails->hoverTracker(), &ItemViewHoverTracker::itemEnter, this,
        [this](const QModelIndex& index)
        {
            if (index.column() == m_descriptionColumnIndex)
            {
                const QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
                if (record && !record->resources.empty())
                    ui->gridDetails->setCursor(Qt::PointingHandCursor);
            }
        });

    connect(ui->gridDetails->hoverTracker(), &ItemViewHoverTracker::itemLeave, this,
        [this]()
        {
            ui->gridDetails->unsetCursor();
        });

    /*
    * Create auto-hiders which will hide empty tables and show a message instead.
    * Tables will be reparented. Snapped scrollbars are already created
    * and will stay in correct parents.
    */
    ItemViewAutoHider::create(ui->gridMaster, tr("No sessions"));
    ItemViewAutoHider::create(ui->gridCameras, tr("No cameras"));

    auto detailsAutoHider = ItemViewAutoHider::create(ui->gridDetails);

    auto updateDetailsEmptyMessage =
        [detailsAutoHider](int currentTab)
        {
            detailsAutoHider->setEmptyViewMessage(currentTab == SessionTab
                ? tr("Select sessions to see their details")
                : tr("Select cameras to see their details"));
        };

    connect(ui->mainTabWidget, &QTabWidget::currentChanged, this, updateDetailsEmptyMessage);
    updateDetailsEmptyMessage(ui->mainTabWidget->currentIndex());

    connect(detailsAutoHider, &ItemViewAutoHider::viewVisibilityChanged,
        m_detailsLabel, &QWidget::setHidden);

    m_detailsLabel->setHidden(detailsAutoHider->isViewHidden());
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
    ui->gridDetails->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->gridDetails->setWordWrap(true);

    setupGridCommon(ui->gridDetails, false);
    m_descriptionColumnIndex = columns.indexOf(QnAuditLogModel::DescriptionColumn);

    ui->gridDetails->horizontalHeader()->setMinimumSectionSize(48);
    ui->gridDetails->horizontalHeader()->setSectionResizeMode(m_descriptionColumnIndex, QHeaderView::Stretch);

    connect(m_itemDelegate, &QnAuditItemDelegate::buttonClicked,        this, &QnAuditLogDialog::at_itemButtonClicked);
    connect(m_itemDelegate, &QnAuditItemDelegate::descriptionClicked,   this, &QnAuditLogDialog::at_descriptionClicked);
}

void QnAuditLogDialog::setupGridCommon(TableView* grid, bool master)
{
    SnappedScrollBar* scrollBar = new SnappedScrollBar(grid->parentWidget());
    grid->setVerticalScrollBar(scrollBar->proxyScrollBar());

    QFont gridFont(font());
    gridFont.setPixelSize(gridFont.pixelSize() - 2);
    grid->setFont(gridFont);

    if (master)
    {
        CheckableHeaderView* header = new CheckableHeaderView(QnAuditLogModel::SelectRowColumn, this);
        header->setVisible(true);
        header->setSectionsClickable(true);
        grid->setHorizontalHeader(header);
        connect(header, &CheckableHeaderView::checkStateChanged, this, &QnAuditLogDialog::at_headerCheckStateChanged);
    }

    grid->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    grid->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    grid->verticalHeader()->setMinimumSectionSize(nx::style::Metrics::kViewRowHeight);
    grid->verticalHeader()->setDefaultSectionSize(nx::style::Metrics::kViewRowHeight);

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

void QnAuditLogDialog::setupContextMenu(TableView* gridMaster)
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

    auto filter =
        [&keywords, &disabledTypes, this](const QnAuditRecord& record)
        {
            if (disabledTypes & record.eventType)
                return false;

            return m_camerasModel->matches(&record, keywords);
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
    {
        if (NX_ASSERT(!record->resources.empty()))
            selectedCameras << record->resources[0];
    }

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

void QnAuditLogDialog::setupFilterCheckbox(
    QCheckBox* checkbox,
    const QColor& activeColor,
    Qn::AuditRecordTypes filteredTypes)
{
    setPaletteColor(checkbox, QPalette::Active, QPalette::Text, activeColor);
    setPaletteColor(checkbox, QPalette::Inactive, QPalette::Text,
        toTransparent(activeColor, kDisabledCheckboxOpacity));
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

    // TODO: #sivanov Get rid of this magic and use common setupTristateCheckbox framework.
    QSignalBlocker blocker(ui->selectAllCheckBox);
    ui->selectAllCheckBox->setCheckState(checkState);

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
        QSignalBlocker blocker(checkbox);
        checkbox->setChecked(state == Qt::Checked);
    }
    at_filterChanged();
};

void QnAuditLogDialog::at_currentTabChanged()
{
    bool allEnabled = (ui->mainTabWidget->currentIndex() == SessionTab);
    const Qn::AuditRecordTypes camerasTypes = Qn::AR_ViewLive | Qn::AR_ViewArchive | Qn::AR_ExportVideo | Qn::AR_CameraUpdate | Qn::AR_CameraInsert | Qn::AR_CameraRemove;

    for(QCheckBox* checkBox: m_filterCheckboxes)
    {
        Qn::AuditRecordTypes eventTypes = static_cast<Qn::AuditRecordTypes>(checkBox->property(checkBoxFilterProperty).toInt());
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
            if (NX_ASSERT(!record.resources.empty()))
            {
                if (filteredCameras.contains(record.resources[0]))
                    cameras.push_back(&record);
            }
        }
        m_camerasModel->setData(cameras);
    }

    ui->mainTabWidget->resize(
        ui->mainTabWidget->sizeHint().width(),
        ui->mainTabWidget->height());
}

void QnAuditLogDialog::at_updateDetailModel()
{
    QString labelText;
    ui->gridDetails->setUpdatesEnabled(false);

    if (ui->mainTabWidget->currentIndex() == SessionTab)
    {
        QnAuditRecordRefList checkedRows = m_sessionModel->checkedRows();
        auto data = filterChildDataBySessions(checkedRows);
        m_detailModel->setData(data);
        m_detailModel->calcColorInterleaving();
        if (m_sessionModel->rowCount() != 0)
        {
            labelText = tr("Details for %n sessions of %1",
                "Total number will be substituted as %1",
                checkedRows.size())
                .arg(m_sessionModel->rowCount());
        }
    }
    else
    {
        QnAuditRecordRefList checkedRows = m_camerasModel->checkedRows();
        auto data = filterChildDataByCameras(checkedRows);
        m_detailModel->setData(data);
        if (m_camerasModel->rowCount() != 0)
        {
            labelText = tr("Details for %n cameras of %1",
                "Total number will be substituted as %1",
                checkedRows.size())
                .arg(m_camerasModel->rowCount());
        }
    }

    m_detailsLabel->setText(labelText);
    m_detailsLabel->resize(m_detailsLabel->sizeHint());

    QStyleOptionViewItem option;
    option.initFrom(ui->gridDetails);

    for (int row = 0; row != m_detailModel->rowCount(); ++row)
    {
        QModelIndex index = m_detailModel->index(row, m_descriptionColumnIndex);
        int height = ui->gridDetails->itemDelegate()->sizeHint(option, index).height();
        ui->gridDetails->setRowHeight(row, height);
    }

    ui->gridDetails->setUpdatesEnabled(true);
}

void QnAuditLogDialog::setupCheckboxes()
{
    m_filterCheckboxes.clear();
    // TODO: separate MitM attack check box.
    setupFilterCheckbox(
        ui->checkBoxLogin,
        colorTheme()->color("auditLog.actions.login"),
        Qn::AR_UnauthorizedLogin | Qn::AR_Login | Qn::AR_MitmAttack);
    setupFilterCheckbox(ui->checkBoxUsers,
        colorTheme()->color("auditLog.actions.updateUser"),
        Qn::AR_UserUpdate | Qn::AR_UserRemove);
    setupFilterCheckbox(
        ui->checkBoxLive,
        colorTheme()->color("auditLog.actions.watchingLive"),
        Qn::AR_ViewLive);
    setupFilterCheckbox(
        ui->checkBoxArchive,
        colorTheme()->color("auditLog.actions.watchingArchive"),
        Qn::AR_ViewArchive);
    setupFilterCheckbox(
        ui->checkBoxExport,
        colorTheme()->color("auditLog.actions.exportVideo"),
        Qn::AR_ExportVideo);
    setupFilterCheckbox(ui->checkBoxCameras,
        colorTheme()->color("auditLog.actions.updateCamera"),
        Qn::AR_CameraUpdate | Qn::AR_CameraRemove | Qn::AR_CameraInsert);
    setupFilterCheckbox(ui->checkBoxSystem,
        colorTheme()->color("auditLog.actions.system"),
        Qn::AR_SystemNameChanged | Qn::AR_SystemmMerge | Qn::AR_SettingsChange
            | Qn::AR_DatabaseRestore | Qn::AR_UpdateInstall);
    setupFilterCheckbox(ui->checkBoxServers,
        colorTheme()->color("auditLog.actions.updateServer"),
        Qn::AR_ServerUpdate | Qn::AR_ServerRemove | Qn::AR_StorageInsert | Qn::AR_StorageUpdate
            | Qn::AR_StorageRemove);
    setupFilterCheckbox(ui->checkBoxBRules,
        colorTheme()->color("auditLog.actions.rules"),
        Qn::AR_BEventUpdate | Qn::AR_BEventRemove | Qn::AR_BEventReset);
    setupFilterCheckbox(
        ui->checkBoxEmail,
        colorTheme()->color("auditLog.actions.emailSettings"),
        Qn::AR_EmailSettings);
};

void setGridGeneralCheckState(QTableView* gridMaster)
{
    if (auto header = qobject_cast<CheckableHeaderView*>(gridMaster->horizontalHeader()))
    {
        QSignalBlocker blocker(header);
        int numSelectedRows = gridMaster->selectionModel()->selectedRows().size();

        header->setCheckState((numSelectedRows == 0) ? Qt::Unchecked :
            ((numSelectedRows < gridMaster->model()->rowCount()) ? Qt::PartiallyChecked : Qt::Checked));
    }
}

void QnAuditLogDialog::at_masterGridSelectionChanged(const QItemSelection& /*selected*/,
    const QItemSelection& /*deselected*/)
{
    if (auto tableView = qobject_cast<QTableView*>(sender()->parent()))
        setGridGeneralCheckState(tableView);
}

void QnAuditLogDialog::at_descriptionClicked(const QModelIndex& index)
{
    if (index.column() != m_descriptionColumnIndex)
        return;

    QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
    if (!record || record->resources.empty())
        return;

    bool showDetail = QnAuditLogModel::hasDetail(record);
    showDetail = !showDetail;
    QnAuditLogModel::setDetail(record, showDetail);

    QStyleOptionViewItem option;
    option.initFrom(ui->gridDetails);
    int height = ui->gridDetails->itemDelegate()->sizeHint(option, index).height();
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
    default:
        break;
    }
}

void QnAuditLogDialog::processPlaybackAction(const QnAuditRecord* record)
{
    QnResourceList resList;
    QnByteArrayConstRef archiveData = record->extractParam("archiveExist");
    size_t i = 0;
    for (const auto& id: record->resources)
    {
        bool archiveExist = archiveData.size() > i && archiveData[i] == '1';
        i++;
        QnResourcePtr res = resourcePool()->getResourceById(id);
        if (res && archiveExist)
            resList << res;
    }

    if (resList.isEmpty())
    {
        QnMessageBox::warning(this, tr("No archive for this position"));
        return;
    }

    QnTimePeriod period;
    period.startTimeMs = record->rangeStartSec * 1000ll;
    period.durationMs = (record->rangeEndSec - record->rangeStartSec) * 1000ll;

    action::Parameters params(resList);
    params.setArgument(Qn::ItemTimeRole, period.startTimeMs);
    if (period.durationMs > 0)
        params.setArgument(Qn::TimePeriodRole, period);

    /* Construct and add a new layout. */
    LayoutResourcePtr layout(new LayoutResource());
    layout->addFlags(Qn::local);
    layout->setIdUnsafe(QnUuid::createUuid());
    layout->setName(tr("Audit trail replay"));

    /* Calculate size of the resulting matrix. */
    qreal desiredItemAspectRatio = QnLayoutResource::kDefaultCellAspectRatio;

    /* Calculate best size for layout cells. */
    qreal desiredCellAspectRatio = desiredItemAspectRatio;

    /* Aspect ratio of the screen free space. */
    QRectF viewportGeometry = display()->boundedViewportGeometry();

    qreal displayAspectRatio = viewportGeometry.isNull()
        ? desiredItemAspectRatio
        : nx::vms::client::core::Geometry::aspectRatio(viewportGeometry);

    if (resList.size() == 1)
    {
        if (QnMediaResourcePtr mediaRes = resList[0].dynamicCast<QnMediaResource>())
        {
            const auto customAspectRatio = mediaRes->customAspectRatio();
            if (customAspectRatio.isValid())
                desiredCellAspectRatio = customAspectRatio.toFloat();
        }
    }

    const int matrixWidth = qMax(1, qRound(std::sqrt(displayAspectRatio * resList.size() / desiredCellAspectRatio)));

    for(int i = 0; i < resList.size(); i++)
    {
        auto resource = resList[i];

        QnLayoutItemData item = layoutItemFromResource(resource);
        item.combinedGeometry = QRect(i % matrixWidth, i / matrixWidth, 1, 1);
        layout->setItemData(
            item.uuid,
            Qn::ItemTimeRole,
            period.startTimeMs);

        layout->addItem(item);
    }

    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredCellAspectRatio);
    layout->setCellAspectRatio(desiredCellAspectRatio);
    layout->setLocalRange(period);
    menu()->trigger(action::OpenInNewTabAction, layout);
}

void QnAuditLogDialog::triggerAction(const QnAuditRecord* record, action::IDType actionId,
    int selectedPage)
{
    QnResourceList resList = resourcePool()->getResourcesByIds(record->resources);
    if (record->eventType == Qn::AR_StorageInsert || record->eventType == Qn::AR_StorageUpdate)
    {
        for (int i = 0; i < resList.size(); ++i)
            resList[i] = resList[i]->getParentResource();
    }
    nx::utils::erase_if(resList, [](const QnResourcePtr& res) { return !res; });

    if (resList.isEmpty())
    {
        const auto count = static_cast<int>(record->resources.size());
        switch (actionId)
        {
            case action::CameraSettingsAction:
                QnMessageBox::warning(this, QnDeviceDependentStrings::getDefaultNameFromSet(
                    resourcePool(),
                    tr("These devices are removed from System", "", count),
                    tr("These cameras are removed from System", "", count)));
                break;
            case action::ServerSettingsAction:
                QnMessageBox::warning(this, tr("These servers are removed from System",
                    "", count));
                break;
            case action::UserSettingsAction:
                QnMessageBox::warning(this, tr("These users are removed from System",
                    "", count));
                break;
            default:
                QnMessageBox::warning(this, tr("These resources are removed from System",
                    "", count));
                break;
        }

        return;
    }

    action::Parameters params(resList);
    params.setArgument(Qn::ItemTimeRole, record->rangeStartSec * 1000ll);
    params.setArgument(Qn::FocusTabRole, selectedPage);
    context()->menu()->trigger(actionId, params);
}

void QnAuditLogDialog::at_itemButtonClicked(const QModelIndex& index)
{
    NX_ASSERT(index.column() == m_descriptionColumnIndex);

    QnAuditRecord* record = index.data(Qn::AuditRecordDataRole).value<QnAuditRecord*>();
    if (!record)
        return;

    if (record->isPlaybackType())
        processPlaybackAction(record);
    else if (record->eventType == Qn::AR_UserUpdate)
        triggerAction(record,
            action::UserSettingsAction,
            QnUserSettingsDialog::SettingsPage);
    else if (record->eventType == Qn::AR_ServerUpdate)
        triggerAction(record,
            action::ServerSettingsAction,
            QnServerSettingsDialog::SettingsPage);
    else if (record->eventType == Qn::AR_StorageUpdate || record->eventType == Qn::AR_StorageInsert)
        triggerAction(record,
            action::ServerSettingsAction,
            QnServerSettingsDialog::StorageManagmentPage);
    else if (record->eventType == Qn::AR_CameraUpdate || record->eventType == Qn::AR_CameraInsert)
        triggerAction(record,
            action::CameraSettingsAction,
            static_cast<int>(CameraSettingsTab::general));

    if (isMaximized())
        showNormal();
}

void QnAuditLogDialog::reset()
{
    disableUpdateData();
    ui->filterLineEdit->clear();
    auto now = QDateTime::currentMSecsSinceEpoch();
    ui->dateRangeWidget->setRange(now, now);
    enableUpdateData();
}

void QnAuditLogDialog::updateData()
{
    if (m_updateDisabled)
    {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    query(ui->dateRangeWidget->startTimeMs(),
          ui->dateRangeWidget->endTimeMs());

    // update UI

    if (!m_requests.isEmpty())
    {
        setCursor(Qt::BusyCursor);
        ui->stackedWidget->setCurrentWidget(ui->progressPage);
    }
    else
    {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_updateDisabled = false;
    m_dirty = false;
}

void QnAuditLogDialog::query(qint64 fromMsec, qint64 toMsec)
{
    using nx::network::rest::UbjsonResult;

    m_requests.clear();

    m_allData.clear();
    m_sessionData.clear();
    m_cameraData.clear();
    m_filteredData.clear();

    // Details model must be cleared first, otherwise it will be rebuilt on `modelReset` signal
    // from the main model, and this will cause invalid index access issue.
    m_detailModel->clearData();
    m_sessionModel->clearData();
    m_camerasModel->clearData();

    if (!connection())
        return;

    auto callback = nx::utils::guarded(this,
        [this](bool success, rest::Handle handle, UbjsonResult result)
        {
            success &= (result.error == UbjsonResult::Result::Error::NoError);
            if (!success)
                NX_WARNING(this, "Audit log cannot be loaded: %1", result.errorString);
            at_gotdata(success, handle, result.deserialized<QnAuditRecordList>());
        });

    const auto onlineServers = resourcePool()->getAllServers(nx::vms::api::ResourceStatus::online);
    for (const QnMediaServerResourcePtr& server: onlineServers)
    {
        auto handle = connectedServerApi()->getAuditLogRecords(
            std::chrono::milliseconds(fromMsec),
            std::chrono::milliseconds(toMsec),
            callback,
            thread(),
            server->getId());

        if (handle > 0)
            m_requests << handle;
    }
}

void QnAuditLogDialog::at_gotdata(bool success, int requestNum, const QnAuditRecordList& records)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (success)
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
                // Group sessions because of different servers may have same session
                // or a single server can duplicate session after restart
                QnAuditRecord& existRecord = itr.value();
                existRecord.rangeStartSec = qMin(existRecord.rangeStartSec, record.rangeStartSec);
                existRecord.rangeEndSec = (existRecord.rangeEndSec == 0 || record.rangeEndSec == 0)
                    ? 0 : qMax(existRecord.rangeEndSec, record.rangeEndSec);
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

    ui->stackedWidget->setCurrentWidget(ui->gridPage);
    setCursor(Qt::ArrowCursor);
}

void QnAuditLogDialog::at_customContextMenuRequested(const QPoint&)
{
    QScopedPointer<QMenu> menu;
    QTableView* gridMaster = (QTableView*) sender();
    QModelIndex idx = gridMaster->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = gridMaster->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        auto manager = context()->menu();

        if (resource)
        {
            action::Parameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, ResourceTree::NodeType::resource);

            menu.reset(manager->newMenu(action::TreeScope, this, parameters));
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu.reset(new QMenu(this));

    m_clipboardAction->setEnabled(gridMaster->selectionModel()->hasSelection());
    m_exportAction->setEnabled(gridMaster->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

TableView* QnAuditLogDialog::currentGridView() const
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
    ui->mainTabWidget->setTabText(kDevicesTabIndex,
        QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Devices"),
            tr("Cameras")
        ));
    ui->checkBoxCameras->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Device actions"),
        tr("Camera actions")
    ));
}

void QnAuditLogDialog::at_exportAction_triggered()
{
    if (currentGridView())
    {
        QnTableExportHelper::exportToFile(
            currentGridView()->model(),
            currentGridView()->selectionModel()->selectedIndexes(),
            this,
            tr("Export selected records to a file"));
    }
}

void QnAuditLogDialog::at_selectAllAction_triggered()
{
    if (currentGridView())
        currentGridView()->selectAll();
}

void QnAuditLogDialog::at_clipboardAction_triggered()
{
    if (currentGridView())
    {
        QnTableExportHelper::copyToClipboard(
            currentGridView()->model(),
            currentGridView()->selectionModel()->selectedIndexes());
    }
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
    // TODO: #sivanov Use showEvent instead.

    if (value && !isVisible())
        updateData();
    QDialog::setVisible(value);
}

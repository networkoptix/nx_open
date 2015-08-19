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
#include <QMouseEvent>
#include "core/resource/layout_resource.h"
#include "ui/common/geometry.h"
#include "ui/style/globals.h"
#include "ui/workbench/workbench_context_aware.h"
#include <ui/workbench/workbench_display.h>
#include "ui/workbench/extensions/workbench_stream_synchronizer.h"
#include <core/resource/user_resource.h>
#include "core/resource/media_resource.h"

namespace {
    const int ProlongedActionRole = Qt::UserRole + 2;
    const int BTN_ICON_SIZE = 16;
    static const int COLUMN_SPACING = 4;
    static const int MAX_DESCR_COL_LEN = 300;
    
    enum MasterGridTabIndex {
        SessionTab,
        CameraTab
    };
}

// --------------------------- QnAuditDetailItemDelegate ------------------------

void QnAuditItemDelegate::setDefaultSectionHeight(int value) 
{ 
    m_defaultSectionHeight = value;
}

QSize QnAuditItemDelegate::sizeHintForText(const QStyleOptionViewItem & option, const QString& textData) const
{
    int width = 0;
    auto itr = m_sizeHintHash.find(textData);
    if (itr != m_sizeHintHash.end())
        width = itr.value();
    else {
        QFontMetrics fm(option.font);
        width = fm.width(textData);
        m_sizeHintHash.insert(textData, width);
    }
    return QSize(width + 4, m_defaultSectionHeight);
}

QSize QnAuditItemDelegate::defaultSizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    QString textData = index.data().toString();
    int width = 0;
    auto itr = m_sizeHintHash.find(textData);
    if (itr != m_sizeHintHash.end())
        width = itr.value();
    else {
        width = base_type::sizeHint(option, index).width();
        m_sizeHintHash.insert(textData, width);
    }
    return QSize(width + 4, m_defaultSectionHeight);
}

QSize QnAuditItemDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    static const QDateTime dateTimePattern = QDateTime::fromString(lit("2015-12-12T23:59:59"), Qt::ISODate);
    static const QString dateTimeStr = dateTimePattern.toString(Qt::DefaultLocaleShortDate);
    static const QString timeStr = dateTimePattern.time().toString(Qt::DefaultLocaleShortDate);
    static const QString dateStr = dateTimePattern.date().toString(Qt::DefaultLocaleShortDate);

    QSize result;
    QnAuditLogModel::Column column = (QnAuditLogModel::Column) index.data(Qn::ColumnDataRole).toInt();
    switch(column)
    {
    case QnAuditLogModel::DescriptionColumn:
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord*>())
            return base_type::sizeHint(option, index);
        QnAuditRecord* record = data.value<QnAuditRecord*>();
        if (record->isPlaybackType() || record->eventType == Qn::AR_CameraUpdate || record->eventType == Qn::AR_CameraInsert)
        {
            bool showDetail = QnAuditLogModel::hasDetail(record);
            if (showDetail || m_widthHint == -1) {
                QTextDocument txtDocument;
                txtDocument.setHtml(index.data(Qn::DisplayHtmlRole).toString());
                result = txtDocument.size().toSize();
                if (m_widthHint == -1)
                    m_widthHint = result.width() - defaultSizeHint(option, index).width(); // extra width because of partially bold data
            }
            else {
                result = defaultSizeHint(option, index);
            }
            if (!showDetail)
                result.setHeight(m_defaultSectionHeight);
        }
        else {
            result = defaultSizeHint(option, index);
        }
        result.setWidth(qMin(result.width(), MAX_DESCR_COL_LEN));
        break;
    }
    case QnAuditLogModel::TimestampColumn:
        result = sizeHintForText(option, dateTimeStr);
        break;
    case QnAuditLogModel::TimeColumn:
        result = sizeHintForText(option, timeStr);
        break;
    case QnAuditLogModel::DateColumn:
        result = sizeHintForText(option, dateStr);
        break;
    case QnAuditLogModel::EndTimestampColumn:
    {
        QSize size1 = sizeHintForText(option, dateTimeStr);
        QSize size2 = sizeHintForText(option, QnAuditLogModel::eventTypeToString(Qn::AR_UnauthorizedLogin));
        result = QSize(qMax(size1.width(), size2.width()), size1.height());
        break;
    }
    case QnAuditLogModel::PlayButtonColumn:
        result = QSize(m_playBottonSize.width() + COLUMN_SPACING*2, m_defaultSectionHeight);
        break;
    case QnAuditLogModel::UserActivityColumn:
    {
        QVariant value = index.data(Qt::SizeHintRole);
        if (value.isValid())
            result = qvariant_cast<QSize>(value);
        else 
            result = defaultSizeHint(option, index);
        break;
    }
    default:
        result = defaultSizeHint(option, index);
    }

    int extraSpaceForColumns[QnAuditLogModel::ColumnCount] = 
    {
        0,                  //SelectRowColumn,
        COLUMN_SPACING+2,   //TimestampColumn,
        COLUMN_SPACING+2,   //EndTimestampColumn,
        0,                  // DurationColumn,
        COLUMN_SPACING,     //UserNameColumn,
        COLUMN_SPACING,     //UserHostColumn,
        COLUMN_SPACING,     // DateColumn,
        COLUMN_SPACING,     // TimeColumn,
        COLUMN_SPACING,     //UserActivityColumn, // not implemented yet
        COLUMN_SPACING,     // EventTypeColumn,
        COLUMN_SPACING,     //DescriptionColumn,
        0,                   //PlayButtonColumn
        COLUMN_SPACING,     // CameraNameColumn,
        COLUMN_SPACING,     // CameraIpColumn,

    };
    result.setWidth(result.width() + extraSpaceForColumns[column]);
    return result;
}

void QnAuditItemDelegate::paintRichDateTime(QPainter * painter, const QStyleOptionViewItem & option, int dateTimeSecs) const
{
    if (dateTimeSecs == 0)
        return;
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(dateTimeSecs * 1000ll);
    QString dateStr = dateTime.date().toString(Qt::DefaultLocaleShortDate) + QString(lit(" "));
    QString timeStr = dateTime.time().toString(Qt::DefaultLocaleShortDate);
    
    QFontMetrics fm(option.font);
    int timeXOffset = fm.size(0, dateStr).width();
    painter->drawText(option.rect, Qt::AlignLeft | Qt::AlignVCenter, dateStr);

    QnScopedPainterFontRollback rollback(painter);
    QFont font = option.font;
    font.setBold(true);
    painter->setFont(font);
    QRect rect(option.rect);
    rect.adjust(timeXOffset, 0, 0, 0);
    painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, timeStr);
}

void QnAuditItemDelegate::paint(QPainter * painter, const QStyleOptionViewItem & _option, const QModelIndex & index) const
{
    QStyleOptionViewItem option = _option;
    if (index.data(Qn::AlternateColorRole).toInt() > 0) 
    {
        const QWidget *widget = option.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        option.features |= QStyleOptionViewItem::Alternate;
        style->drawPrimitive(QStyle::PE_PanelItemViewRow, &option, painter, widget);
    }


    QnAuditLogModel::Column column = (QnAuditLogModel::Column) index.data(Qn::ColumnDataRole).toInt();
    switch (column)
    {
    case QnAuditLogModel::TimestampColumn:
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord*>())
            base_type::paint(painter, option, index);
        const QnAuditRecord* record = data.value<QnAuditRecord*>();
        paintRichDateTime(painter, option, record->createdTimeSec);
        break;
    }
    case QnAuditLogModel::EndTimestampColumn:
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord*>())
            base_type::paint(painter, option, index);
        const QnAuditRecord* record = data.value<QnAuditRecord*>();
        if (record->eventType == Qn::AR_UnauthorizedLogin)
            base_type::paint(painter, option, index);
        else
            paintRichDateTime(painter, option, record->rangeEndSec);
        break;
    }
    case QnAuditLogModel::DescriptionColumn:
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
        break;
    }
    case QnAuditLogModel::PlayButtonColumn:
    {
        QVariant data = index.data(Qn::AuditRecordDataRole);
        if (!data.canConvert<QnAuditRecord*>())
            return base_type::paint(painter, option, index);
        const QnAuditRecord* record = data.value<QnAuditRecord*>();

        QStyleOptionButton button;
        button.text = index.data(Qt::DisplayRole).toString();
        if (button.text.isEmpty())
            return base_type::paint(painter, option, index);
        
        if (option.state & QStyle::State_MouseOver)
            button.icon = index.data(Qn::DecorationHoveredRole).value<QIcon>(); 
        else
            button.icon = index.data(Qt::DecorationRole).value<QIcon>();
        button.iconSize = QSize(BTN_ICON_SIZE, BTN_ICON_SIZE);
        button.state = option.state;

        button.rect.setTopLeft(option.rect.topLeft() + QPoint(4, 4));
        button.rect.setSize(m_playBottonSize);
        QApplication::style()->drawControl(QStyle::CE_PushButton, &button, painter);
        break;
    }
    case QnAuditLogModel::UserActivityColumn:
    {
        // draw chart manually
        qreal chartData = index.data(Qn::AuditLogChartDataRole).toReal();
        QColor chartColor = index.data(Qt::BackgroundColorRole).value<QColor>();

        const int shift = 16;
        if (option.state & QStyle::State_Selected)
            chartColor = shiftColor(chartColor, shift, shift, shift); // alternate row color

        option.rect.adjust(2, 1, -2, -1);
        painter->fillRect(QRect(option.rect.left() , option.rect.top(), option.rect.width() * chartData, option.rect.height()), chartColor);
        if (option.state & QStyle::State_MouseOver) {
            QnScopedPainterFontRollback rollback(painter);
            QnScopedPainterPenRollback rollback2(painter);
            painter->setFont(option.font);
            painter->setPen(option.palette.foreground().color());
            painter->drawText(option.rect, Qt::AlignLeft | Qt::AlignVCenter, index.data().toString());
        }
        break;
    }
    default:
        base_type::paint(painter, option, index);
    }
}

// ---------------------- QnAuditLogDialog ------------------------------

QnAuditRecordRefList QnAuditLogDialog::applyFilter()
{
    QnAuditRecordRefList result;
    QStringList keywords = ui->filterLineEdit->text().split(lit(" "));

    Qn::AuditRecordTypes disabledTypes = Qn::AR_NotDefined;
    for (const QCheckBox* checkBox: m_filterCheckboxes) 
    {
        if (!checkBox->isChecked()) 
            disabledTypes |= (Qn::AuditRecordTypes) checkBox->property("filter").toInt();
    }


    auto filter = [&keywords, &disabledTypes] (const QnAuditRecord& record) 
    {
        if (disabledTypes & record.eventType)
            return false;

        bool matched = true;
        QString wholeText = QnAuditLogModel::makeSearchPattern(&record);
        for (const auto& keyword: keywords) {
            if (!wholeText.contains(keyword, Qt::CaseInsensitive)) {
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
    QSet<QnUuid> selectedSessions;
    for (const QnAuditRecord* record: checkedRows) 
        selectedSessions << record->authSession.id;

    QnAuditRecordRefList result;
    auto filter = [&selectedSessions] (const QnAuditRecord* record) {
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
        for (const auto& id: record->resources) {
            if (selectedCameras.contains(id))
                return true;
        }
        return false;
    };
    std::copy_if(m_filteredData.begin(), m_filteredData.end(), std::back_inserter(result), filter);
    return result;
}

QSize QnAuditLogDialog::calcButtonSize(const QFont& font) const
{
    std::unique_ptr<QPushButton> button(new QPushButton());
    button->setText(tr("Play this"));
    QSize result = button->minimumSizeHint();
    result.setWidth(result.width() + BTN_ICON_SIZE);
    return result;
}

QList<QnAuditLogModel::Column> detailSessionColumns(bool showUser)
{
    QList<QnAuditLogModel::Column> columns;
    columns << 
        QnAuditLogModel::DateColumn <<
        QnAuditLogModel::TimeColumn;
    if (showUser)
        columns << QnAuditLogModel::UserNameColumn << QnAuditLogModel::UserHostColumn;
    columns <<
        QnAuditLogModel::EventTypeColumn <<
        QnAuditLogModel::DescriptionColumn <<
        QnAuditLogModel::PlayButtonColumn;
    
    return columns;
}

void QnAuditLogDialog::setupFilterCheckbox(QCheckBox* checkbox, const QColor& color, Qn::AuditRecordTypes filteredTypes)
{
    QPalette palette = checkbox->palette();
    palette.setColor(checkbox->foregroundRole(), color);
    checkbox->setPalette(palette);
    checkbox->setProperty("filter", (int) filteredTypes);
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

    ui->selectAllCheckBox->blockSignals(true);
    ui->selectAllCheckBox->setCheckState(checkState);
    ui->selectAllCheckBox->blockSignals(false);

    at_filterChanged();
};

void QnAuditLogDialog::at_selectAllCheckboxChanged()
{
    Qt::CheckState state = ui->selectAllCheckBox->checkState();
    if (state == Qt::PartiallyChecked) {
        ui->selectAllCheckBox->setChecked(true);
        return;
    }

    for (const auto& checkbox: m_filterCheckboxes)
    {
        checkbox->blockSignals(true);
        checkbox->setChecked(state == Qt::Checked);
        checkbox->blockSignals(false);
    }
    at_filterChanged();
};

void QnAuditLogDialog::at_currentTabChanged()
{
    bool allEnabled = (ui->tabWidget->currentIndex() == SessionTab);
    const Qn::AuditRecordTypes camerasTypes = Qn::AR_ViewLive | Qn::AR_ViewArchive | Qn::AR_ExportVideo | Qn::AR_CameraUpdate | Qn::AR_CameraInsert | Qn::AR_CameraRemove;

    for(QCheckBox* checkBox: m_filterCheckboxes) {
        Qn::AuditRecordTypes eventTypes = (Qn::AuditRecordTypes) checkBox->property("filter").toInt();
        checkBox->setEnabled(allEnabled || (camerasTypes & eventTypes));
    }
    at_filterChanged();
}

void QnAuditLogDialog::at_filterChanged()
{
    m_filteredData = applyFilter();

    if (ui->tabWidget->currentIndex() == SessionTab)
    {
        QSet<QnUuid> filteredIdList;
        for (const QnAuditRecord* record: m_filteredData)
            filteredIdList << record->authSession.id;
    
        QnAuditRecordRefList filteredSessions;
        for (auto& record: m_sessionData) {
            if (filteredIdList.contains(record.authSession.id))
                filteredSessions.push_back(&record);
        }

        m_sessionModel->setData(filteredSessions);
    }
    else {
        QSet<QnUuid> filteredCameras;
        for (const QnAuditRecord* record: m_filteredData) {
            for (const auto& id: record->resources)
            filteredCameras << id;
        }

        QnAuditRecordRefList cameras;
        for (auto& record: m_cameraData) {
            if (filteredCameras.contains(record.resources[0]))
                cameras.push_back(&record);
        }
        m_camerasModel->setData(cameras);
    }
}

void QnAuditLogDialog::at_updateDetailModel()
{
    if (ui->tabWidget->currentIndex() == SessionTab)
    {
        QnAuditRecordRefList checkedRows = m_sessionModel->checkedRows();
        auto data = filterChildDataBySessions(checkedRows);
        m_detailModel->setData(data);
        m_detailModel->calcColorInterleaving();
    }
    else {
        QnAuditRecordRefList checkedRows = m_camerasModel->checkedRows();
        auto data = filterChildDataByCameras(checkedRows);
        m_detailModel->setData(data);
    }
}

int calcHeaderHeight(QHeaderView* header)
{
    // otherwise use the contents
    QStyleOptionHeader opt;

    opt.initFrom(header);
    opt.state = QStyle::State_None | QStyle::State_Raised;
    opt.orientation = Qt::Horizontal;
    opt.state |= QStyle::State_Horizontal;
    opt.state |= QStyle::State_Enabled;
    opt.section = 0;

    QFont fnt = header->font();
    fnt.setBold(true);
    opt.fontMetrics = QFontMetrics(fnt);
    QSize size = header->style()->sizeFromContents(QStyle::CT_HeaderSection, &opt, QSize(), header);

    if (header->isSortIndicatorShown())
    {
        int margin = header->style()->pixelMetric(QStyle::PM_HeaderMargin, &opt, header);
        size.setHeight(size.height() +  margin);
    }
    return size.height();
}

void QnAuditLogDialog::at_updateCheckboxes()
{
    m_filterCheckboxes.clear();
    QnAuditLogColors colors = m_sessionModel->colors();
    setupFilterCheckbox(ui->checkBoxLogin, colors.loginAction, Qn::AR_UnauthorizedLogin | Qn::AR_Login);
    setupFilterCheckbox(ui->checkBoxUsers, colors.updUsers, Qn::AR_UserUpdate | Qn::AR_UserRemove);
    setupFilterCheckbox(ui->checkBoxLive, colors.watchingLive, Qn::AR_ViewLive);
    setupFilterCheckbox(ui->checkBoxArchive, colors.watchingArchive, Qn::AR_ViewArchive);
    setupFilterCheckbox(ui->checkBoxExport, colors.exportVideo, Qn::AR_ExportVideo);
    setupFilterCheckbox(ui->checkBoxCameras, colors.updCamera, Qn::AR_CameraUpdate | Qn::AR_CameraRemove | Qn::AR_CameraInsert);
    setupFilterCheckbox(ui->checkBoxSystem, colors.systemActions, Qn::AR_SystemNameChanged | Qn::AR_SystemmMerge | Qn::AR_SettingsChange | Qn::AR_DatabaseRestore);
    setupFilterCheckbox(ui->checkBoxServers, colors.updServer, Qn::AR_ServerUpdate | Qn::AR_ServerRemove);
    setupFilterCheckbox(ui->checkBoxBRules, colors.eventRules, Qn::AR_BEventUpdate | Qn::AR_BEventRemove | Qn::AR_BEventReset);
    setupFilterCheckbox(ui->checkBoxEmail, colors.emailSettings, Qn::AR_EmailSettings);
};

void setGridGeneralCheckState(QTableView* gridMaster, Qt::CheckState checkState)
{
    QnCheckBoxedHeaderView* headers = (QnCheckBoxedHeaderView*) gridMaster->horizontalHeader();
    QnAuditLogModel* model = (QnAuditLogModel*) gridMaster->model();
    headers->blockSignals(true);
    headers->setCheckState(checkState);
    headers->blockSignals(false);
}

void QnAuditLogDialog::at_masterGridSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QnTableView* gridMaster = (QnTableView*) sender()->parent();
    QModelIndex mouseIdx = gridMaster->mouseIndex();
    QnAuditLogModel* model = (QnAuditLogModel*) gridMaster->model();
    bool isCheckRow = mouseIdx.isValid() && mouseIdx.column() == 0;
    if (isCheckRow) {
        if (mouseIdx.data(Qt::CheckStateRole) == Qt::Unchecked)
            m_skipNextPressSignal = true;
    }
    else {
        model->blockSignals(true);
        model->setCheckState(Qt::Unchecked);
        model->blockSignals(false);
    }

    QModelIndexList indexes = gridMaster->selectionModel()->selectedRows();
    if (m_skipNextSelIndex.isValid()) {
        for (auto itr = indexes.begin(); itr != indexes.end(); ++itr) {
            if (*itr == m_skipNextSelIndex) {
                indexes.erase(itr);
                m_skipNextPressSignal = false;
                break;
            }
        }
    }
    m_skipNextSelIndex = QModelIndex();
    model->setData(indexes, Qt::Checked, Qt::CheckStateRole);
    setGridGeneralCheckState(gridMaster, model->checkState());
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
    ui->gridMaster->setModel(m_sessionModel);

    setupMasterGridCommon(ui->gridMaster);
}

void QnAuditLogDialog::setupMasterGridCommon(QnTableView* gridMaster)
{
    QnAuditLogModel* model = static_cast<QnAuditLogModel*>(gridMaster->model());

    gridMaster->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    QnCheckBoxedHeaderView* headers = new QnCheckBoxedHeaderView(this);
    headers->setVisible(true);
    headers->setSectionsClickable(true);
    headers->setSectionResizeMode(QHeaderView::ResizeToContents);

    gridMaster->setHorizontalHeader(headers);
    gridMaster->setItemDelegate(m_itemDelegate);
    gridMaster->setMouseTracking(true);
    model->setheaderHeight(calcHeaderHeight(gridMaster->horizontalHeader()));

    connect(model, &QAbstractItemModel::dataChanged, this, &QnAuditLogDialog::at_updateDetailModel);
    connect(model, &QAbstractItemModel::modelReset, this, &QnAuditLogDialog::at_updateDetailModel);
    connect (headers,           &QnCheckBoxedHeaderView::checkStateChanged, this, &QnAuditLogDialog::at_headerCheckStateChanged);
    connect(gridMaster->selectionModel(), &QItemSelectionModel::selectionChanged, this, &QnAuditLogDialog::at_masterGridSelectionChanged);
    connect(gridMaster,         &QTableView::pressed, this, &QnAuditLogDialog::at_masterItemPressed); // put selection changed before item pressed
    connect(gridMaster,         &QTableView::clicked,               this,   &QnAuditLogDialog::at_sessionsGrid_clicked);

    setupContextMenu(gridMaster);
}

void QnAuditLogDialog::setupContextMenu(QTableView* gridMaster)
{
    gridMaster->addAction(m_clipboardAction);
    gridMaster->addAction(m_exportAction);
    connect(gridMaster,         &QTableView::customContextMenuRequested, this, &QnAuditLogDialog::at_sessionsGrid_customContextMenuRequested);
    connect(m_selectAllAction,  &QAction::triggered,  this, &QnAuditLogDialog::at_selectAllAction_triggered);
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
    ui->gridCameras->setModel(m_camerasModel);

    setupMasterGridCommon(ui->gridCameras);
    ui->gridCameras->horizontalHeader()->setStretchLastSection(true);
}

QnAuditLogDialog::QnAuditLogDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::AuditLogDialog),
    m_updateDisabled(false),
    m_dirty(false),
    m_skipNextPressSignal(false)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    //setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);

    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);


    m_itemDelegate = new QnAuditItemDelegate(this);
    m_itemDelegate->setPlayButtonSize(calcButtonSize(ui->gridMaster->font()));
    m_itemDelegate->setDefaultSectionHeight(ui->gridDetails->verticalHeader()->defaultSectionSize());

    setupSessionsGrid();
    setupCamerasGrid();
    connect(m_sessionModel, &QnAuditLogModel::colorsChanged, this, &QnAuditLogDialog::at_updateCheckboxes);
    at_updateCheckboxes();
    connect(ui->selectAllCheckBox, &QCheckBox::stateChanged, this, &QnAuditLogDialog::at_selectAllCheckboxChanged);

    // setup detail grid

    m_detailModel = new QnAuditLogDetailModel(this);
    m_detailModel->setColumns(detailSessionColumns(true));
    ui->gridDetails->setModel(m_detailModel);
    ui->gridDetails->setItemDelegate(m_itemDelegate);
    ui->gridDetails->setWordWrap(true);
    m_detailModel->setheaderHeight(calcHeaderHeight(ui->gridDetails->horizontalHeader()));

    ui->gridDetails->horizontalHeader()->setMinimumSectionSize(48);
    
    ui->gridDetails->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->gridDetails->horizontalHeader()->setStretchLastSection(true);

    ui->gridDetails->setMouseTracking(true);


    connect(ui->gridDetails, &QTableView::pressed, this, &QnAuditLogDialog::at_ItemPressed);
    connect(ui->gridDetails, &QTableView::entered, this, &QnAuditLogDialog::at_ItemEntered);

    connect (ui->gridDetails->horizontalHeader(), &QHeaderView::sectionResized, this, 
        [this] (int logicalIndex, int oldSize, int newSize) 
        {
            int w = 0;
            const QHeaderView* headers = ui->gridDetails->horizontalHeader();
            for (int i = 0; i < headers->count() - 1; ++i)
                w += headers->sectionSize(i);
            w += m_itemDelegate->playButtonSize().width() + COLUMN_SPACING*2;
            w += ui->gridDetails->verticalScrollBar()->width() + 2;
            bool needAdjust =  ui->gridDetails->width() < w;
            ui->gridDetails->setMinimumSize(w, -1);
            if (needAdjust)
                ui->gridDetails->adjustSize();
        }, Qt::QueuedConnection
    );
    setupContextMenu(ui->gridDetails);

    QDate dt = QDateTime::currentDateTime().date();
    ui->dateEditFrom->setDate(dt);
    ui->dateEditTo->setDate(dt);

    
    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);

    ui->refreshButton->setIcon(qnSkin->icon("refresh.png"));
    ui->loadingProgressBar->hide();

    connect(m_clipboardAction,      &QAction::triggered,                this,   &QnAuditLogDialog::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                this,   &QnAuditLogDialog::at_exportAction_triggered);

    connect(ui->dateEditFrom,       &QDateEdit::dateChanged,            this,   &QnAuditLogDialog::updateData);
    connect(ui->dateEditTo,         &QDateEdit::dateChanged,            this,   &QnAuditLogDialog::updateData);
    connect(ui->refreshButton,      &QAbstractButton::clicked,          this,   &QnAuditLogDialog::updateData);


    connect(ui->gridDetails,         &QTableView::clicked,               this,   &QnAuditLogDialog::at_eventsGrid_clicked);

    ui->mainGridLayout->activate();
    
    ui->filterLineEdit->setPlaceholderText(tr("Search"));
    connect(ui->filterLineEdit, &QLineEdit::textChanged, this, &QnAuditLogDialog::at_filterChanged);
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, &QnAuditLogDialog::at_currentTabChanged);
    /*
    connect(ui->tabWidget, &QTabWidget::currentChanged, 
        this, [this](int index) {
            if (index == SessionTab)
                ui->gridMaster->adjustSize();
            else
                ui->gridCameras->adjustSize();
            at_filterChanged();
        }
    );
    */
    
}

QnAuditLogDialog::~QnAuditLogDialog() {
}

void QnAuditLogDialog::at_eventsGrid_clicked(const QModelIndex& index)
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

void QnAuditLogDialog::at_ItemEntered(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) == QnAuditLogModel::DescriptionColumn)
        ui->gridDetails->setCursor(Qt::PointingHandCursor);
    else
        ui->gridDetails->setCursor(Qt::ArrowCursor);
}

void QnAuditLogDialog::at_headerCheckStateChanged(Qt::CheckState state)
{
    QHeaderView* headers = (QHeaderView*) sender();
    QTableView* masterGrid = (QTableView*) headers->parent();
    QnAuditLogModel* model = (QnAuditLogModel*) masterGrid->model();
    model->setCheckState(state);
}

void QnAuditLogDialog::at_masterItemPressed(const QModelIndex& index)
{
    QnTableView* gridMaster = (QnTableView*) sender();

    if (m_skipNextPressSignal) {
        m_skipNextPressSignal = false;
        m_skipNextSelIndex = QModelIndex();
        return;
    }
    m_lastPressIndex = index;
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::SelectRowColumn)
        return;

    m_skipNextSelIndex = index;

    Qt::CheckState checkState = (Qt::CheckState) index.data(Qt::CheckStateRole).toInt();
    if (checkState == Qt::Checked)
        checkState = Qt::Unchecked;
    else
        checkState = Qt::Checked;
    QnAuditLogModel* model = (QnAuditLogModel*) gridMaster->model();
    model->setData(index, checkState, Qt::CheckStateRole);
    QnCheckBoxedHeaderView* headers = (QnCheckBoxedHeaderView*) gridMaster->horizontalHeader();
    headers->blockSignals(true);
    headers->setCheckState(model->checkState());
    headers->blockSignals(false);
}

void QnAuditLogDialog::processPlaybackAction(const QnAuditRecord* record)
{
    QnResourceList resList;
    QnByteArrayConstRef archiveData = record->extractParam("archiveExist");
    int i = 0;
    for (const auto& id: record->resources) {
        bool archiveExist = archiveData.size() > i && archiveData[i] == '1';
        i++;
        QnResourcePtr res = qnResPool->getResourceById(id);
        if (res && archiveExist)
            resList << res;
    }

    QnActionParameters params(resList);
    if (resList.isEmpty()) {
        QMessageBox::information(this, tr("Information"), tr("No archive data for that position left"));
        return;
    }

    QnTimePeriod period;
    period.startTimeMs = record->rangeStartSec * 1000ll;
    period.durationMs = (record->rangeEndSec - record->rangeStartSec) * 1000ll;

    params.setArgument(Qn::ItemTimeRole, period.startTimeMs);
    if (period.durationMs > 0)
        params.setArgument(Qn::TimePeriodRole, period);
    

    /* Construct and add a new layout. */
    QnLayoutResourcePtr layout(new QnLayoutResource(qnResTypePool));
    //layout->addFlags(Qn::local);
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

    if (resList.size() == 1) {
        if (QnMediaResourcePtr mediaRes = resList[0].dynamicCast<QnMediaResource>()) {
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

    layout->setData(Qn::LayoutTimeLabelsRole, true);
    //layout->setData(Qn::LayoutSyncStateRole, QVariant::fromValue<QnStreamSynchronizationState>(QnStreamSynchronizationState()));
    layout->setData(Qn::LayoutPermissionsRole, static_cast<int>(Qn::ReadPermission));
    layout->setData(Qn::LayoutCellAspectRatioRole, desiredCellAspectRatio);
    layout->setCellAspectRatio(desiredCellAspectRatio);
    layout->setLocalRange(period);

    resourcePool()->addResource(layout);
    menu()->trigger(Qn::OpenSingleLayoutAction, layout);

}

void QnAuditLogDialog::triggerAction(const QnAuditRecord* record, Qn::ActionId ActionId, const QString& objectName)
{
    QnResourceList resList;
    for (const auto& id: record->resources) {
        if (QnResourcePtr res = qnResPool->getResourceById(id))
            resList << res;
    }

    QnActionParameters params(resList);
    if (resList.isEmpty()) {
        QMessageBox::information(this, tr("Information"), tr("This resources already removed from the system"));
        return;
    }
    
    params.setArgument(Qn::ItemTimeRole, record->rangeStartSec * 1000ll);
    context()->menu()->trigger(ActionId, params);
}

void QnAuditLogDialog::at_ItemPressed(const QModelIndex& index)
{
    if (index.data(Qn::ColumnDataRole) != QnAuditLogModel::PlayButtonColumn)
        return;

    QVariant data = index.data(Qn::AuditRecordDataRole);
    if (!data.canConvert<QnAuditRecord*>())
        return;
    QnAuditRecord* record = data.value<QnAuditRecord*>();
    if (record->isPlaybackType())
        processPlaybackAction(record);
    else if (record->eventType == Qn::AR_UserUpdate)
        triggerAction(record, Qn::UserSettingsAction,   tr("user(s)"));
    else if (record->eventType == Qn::AR_ServerUpdate)
        triggerAction(record, Qn::ServerSettingsAction, tr("server(s)"));
    else if (record->eventType == Qn::AR_CameraUpdate || record->eventType == Qn::AR_CameraInsert)
        triggerAction(record, Qn::CameraSettingsAction,   tr("camera(s)"));

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
        ui->gridCameras->setDisabled(true);
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
    m_sessionModel->clearData();
    m_camerasModel->clearData();
    m_detailModel->clearData();
    m_requests.clear();
    
    m_allData.clear();
    m_sessionData.clear();
    m_cameraData.clear();
    m_filteredData.clear();


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

void QnAuditLogDialog::makeSessionData()
{
    m_sessionData.clear();
    QMap<QnUuid, int> activityPerSession;
    for (const QnAuditRecord& record: m_allData)
    {
        if (record.isLoginType())
            m_sessionData << record;
        activityPerSession[record.authSession.id]++;
    }

    for (QnAuditRecord& record: m_sessionData)
        record.addParam("childCnt", QByteArray::number(activityPerSession.value(record.authSession.id)));
}

void QnAuditLogDialog::makeCameraData()
{
    m_cameraData.clear();
    QMap<QnUuid, int> activityPerCamera;
    for (const QnAuditRecord& record: m_allData)
    {
        if (record.isPlaybackType() || record.isCameraType()) {
            for (const QnUuid& cameraId: record.resources)
                activityPerCamera[cameraId]++;
        }
    }
    for (auto itr = activityPerCamera.begin(); itr != activityPerCamera.end(); ++itr) {
        QnAuditRecord cameraRecord;
        cameraRecord.resources.push_back(itr.key());
        cameraRecord.addParam("childCnt", QByteArray::number(itr.value())); // used for "user activity" column
        m_cameraData.push_back(cameraRecord);
    }
}

void QnAuditLogDialog::requestFinished()
{
    setGridGeneralCheckState(ui->gridMaster, Qt::Unchecked);
    setGridGeneralCheckState(ui->gridCameras, Qt::Unchecked);
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
    QTableView* gridMaster = (QTableView*) sender();
    QModelIndex idx = gridMaster->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = gridMaster->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
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
    else if (ui->tabWidget->currentIndex() == SessionTab)
        return ui->gridMaster;
    else
        return ui->gridCameras;
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

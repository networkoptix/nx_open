// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>

#include <api/model/legacy_audit_record.h>
#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnAuditLogMasterModel;
class QnAuditLogDetailModel;
class QnAuditItemDelegate;
class QnAuditLogModel;
struct QnTimePeriod;
class QLabel;

namespace Ui { class AuditLogDialog; }
namespace nx::vms::client::desktop { class TableView; }

class QnAuditLogDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    enum MasterGridTabIndex
    {
        sessionTabIndex,
        cameraTabIndex
    };

public:
    explicit QnAuditLogDialog(QWidget* parent);
    virtual ~QnAuditLogDialog();

    void disableUpdateData();
    void enableUpdateData();

    void setSearchText(const QString& text);
    void setSearchPeriod(const QnTimePeriod& period);
    void setFocusTab(MasterGridTabIndex tabIndex);

protected:
    virtual void setVisible(bool value) override;

private slots:
    void reset();
    void updateData();
    void at_gotdata(
        bool success, rest::Handle requestNum, nx::Uuid serverId, QnLegacyAuditRecordList data);
    void at_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_selectAllAction_triggered();
    void at_itemButtonClicked(const QModelIndex& index);
    void at_descriptionClicked(const QModelIndex& idx);
    void at_headerCheckStateChanged(Qt::CheckState state);
    void at_updateDetailModel();
    void at_typeCheckboxChanged();
    void at_filterChanged();
    void at_currentTabChanged();
    void at_selectAllCheckboxChanged(Qt::CheckState state);
    void at_masterGridSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

private:
    void requestFinished();

    /**
     * Get data from server
     *
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 fromMsec, qint64 toMsec);

    void setupSessionsGrid();
    void setupCamerasGrid();
    void setupDetailsGrid();
    void setupCheckboxes();

    void setupGridCommon(nx::vms::client::desktop::TableView* grid, bool master);

    QnLegacyAuditRecordRefList filterChildDataBySessions(const QnLegacyAuditRecordRefList& checkedRows);
    QnLegacyAuditRecordRefList filterChildDataByCameras(const QnLegacyAuditRecordRefList& checkedRows);
    void setupFilterCheckbox(
        QCheckBox* checkbox,
        const QColor& activeColor,
        Qn::LegacyAuditRecordTypes filteredTypes);
    void processPlaybackAction(const QnLegacyAuditRecord* record);
    void triggerAction(const QnLegacyAuditRecord* record, nx::vms::client::desktop::menu::IDType ActionId, int selectedPage);
    QnLegacyAuditRecordRefList applyFilter();
    void makeSessionData();
    void makeCameraData();
    void setupContextMenu(nx::vms::client::desktop::TableView* gridMaster);
    nx::vms::client::desktop::TableView* currentGridView() const;

    void retranslateUi();

private:
    QScopedPointer<Ui::AuditLogDialog> ui;

    QnAuditLogMasterModel* m_sessionModel;
    QnAuditLogModel* m_camerasModel;
    QnAuditLogDetailModel* m_detailModel;
    QSet<rest::Handle> m_requests;

    QnLegacyAuditRecordList m_allData;
    QnLegacyAuditRecordList m_cameraData;
    QnLegacyAuditRecordList m_sessionData;
    QnLegacyAuditRecordRefList m_filteredData;
    bool m_updateDisabled;
    bool m_dirty;

    QAction* m_selectAllAction;
    QAction* m_exportAction;
    QAction* m_clipboardAction;
    QList<QCheckBox*> m_filterCheckboxes;

    QLabel* m_detailsLabel;

    QnAuditItemDelegate* m_itemDelegate;
    int m_descriptionColumnIndex;
};

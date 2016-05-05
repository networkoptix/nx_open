#pragma once

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/workbench_state_dependent_dialog.h>
#include "api/model/audit/audit_record.h"
#include "ui/actions/actions.h"

class QnAuditLogMasterModel;
class QnAuditLogDetailModel;
class QnAuditItemDelegate;
class QnTableView;
class QnAuditLogModel;

namespace Ui {
    class AuditLogDialog;
}

class QnAuditLogDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnAuditLogDialog(QWidget *parent);
    virtual ~QnAuditLogDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(const QDate& from, const QDate& to);
protected:
    void setVisible(bool value) override;

private slots:
    void updateData();
    void at_gotdata(int httpStatus, const QnAuditRecordList& data, int requestNum);
    void at_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_selectAllAction_triggered();
    void at_itemButtonClicked(const QModelIndex& index);
    void at_descriptionPressed(const QModelIndex& idx);
    void at_headerCheckStateChanged(Qt::CheckState state);
    void at_updateDetailModel();
    void at_typeCheckboxChanged();
    void at_filterChanged();
    void at_currentTabChanged();
    void at_selectAllCheckboxChanged();
    void at_updateCheckboxes();
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

    void setupGridCommon(QnTableView* grid, bool master);

    QnAuditRecordRefList filterChildDataBySessions(const QnAuditRecordRefList& checkedRows);
    QnAuditRecordRefList filterChildDataByCameras(const QnAuditRecordRefList& checkedRows);
    void setupFilterCheckbox(QCheckBox* checkbox, const QColor& color, Qn::AuditRecordTypes filteredTypes);
    void processPlaybackAction(const QnAuditRecord* record);
    void triggerAction(const QnAuditRecord* record, QnActions::IDType ActionId);
    QnAuditRecordRefList applyFilter();
    void makeSessionData();
    void makeCameraData();
    void setupContextMenu(QTableView* gridMaster);
    QTableView* currentGridView() const;

    void retranslateUi();

private:
    QScopedPointer<Ui::AuditLogDialog> ui;

    QnAuditLogMasterModel *m_sessionModel;
    QnAuditLogModel *m_camerasModel;
    QnAuditLogDetailModel *m_detailModel;
    QSet<int> m_requests;

    QnAuditRecordList m_allData;
    QnAuditRecordList m_cameraData;
    QnAuditRecordList m_sessionData;
    QnAuditRecordRefList m_filteredData;
    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    QList<QCheckBox*> m_filterCheckboxes;

    QnAuditItemDelegate* m_itemDelegate;
};


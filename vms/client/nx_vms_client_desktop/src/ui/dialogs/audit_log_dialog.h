// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <QtGui/QStandardItem>

#include <QtWidgets/QDialog>

#include <api/model/audit/audit_record.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnAuditLogMasterModel;
class QnAuditLogDetailModel;
class QnAuditItemDelegate;
class QnAuditLogModel;
class QLabel;

namespace Ui { class AuditLogDialog; }
namespace nx::vms::client::desktop { class TableView; }

class QnAuditLogDialog: public QnSessionAwareDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareDialog;

public:
    explicit QnAuditLogDialog(QWidget* parent);
    virtual ~QnAuditLogDialog();

    void disableUpdateData();
    void enableUpdateData();

protected:
    virtual void setVisible(bool value) override;

private slots:
    void reset();
    void updateData();
    void at_gotdata(bool success, int requestNum, const QnAuditRecordList& data);
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
    void at_selectAllCheckboxChanged();
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

    QnAuditRecordRefList filterChildDataBySessions(const QnAuditRecordRefList& checkedRows);
    QnAuditRecordRefList filterChildDataByCameras(const QnAuditRecordRefList& checkedRows);
    void setupFilterCheckbox(
        QCheckBox* checkbox,
        const QColor& activeColor,
        Qn::AuditRecordTypes filteredTypes);
    void processPlaybackAction(const QnAuditRecord* record);
    void triggerAction(const QnAuditRecord* record, nx::vms::client::desktop::ui::action::IDType ActionId, int selectedPage);
    QnAuditRecordRefList applyFilter();
    void makeSessionData();
    void makeCameraData();
    void setupContextMenu(nx::vms::client::desktop::TableView* gridMaster);
    nx::vms::client::desktop::TableView* currentGridView() const;

    virtual void retranslateUi() override;

private:
    QScopedPointer<Ui::AuditLogDialog> ui;

    QnAuditLogMasterModel* m_sessionModel;
    QnAuditLogModel* m_camerasModel;
    QnAuditLogDetailModel* m_detailModel;
    QSet<int> m_requests;

    QnAuditRecordList m_allData;
    QnAuditRecordList m_cameraData;
    QnAuditRecordList m_sessionData;
    QnAuditRecordRefList m_filteredData;
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


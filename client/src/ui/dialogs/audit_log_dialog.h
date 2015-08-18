#ifndef QN_AUDIT_LOG_DIALOG_H
#define QN_AUDIT_LOG_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "api/model/audit/audit_record.h"
#include "ui/actions/actions.h"

class QnAuditLogMasterModel;
class QnAuditLogDetailModel;
class QnCheckBoxedHeaderView;
class QnAuditItemDelegate;
class QnTableView;
class QnAuditLogModel;

namespace Ui {
    class AuditLogDialog;
}

class QnAuditItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnAuditItemDelegate(QObject *parent = NULL): base_type(parent), m_defaultSectionHeight(0), m_widthHint(-1) {}
    virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;
    void setPlayButtonSize(const QSize& size) { m_playBottonSize = size; }
    QSize playButtonSize() const { return m_playBottonSize; }
    void setDefaultSectionHeight(int value);
private:
    void paintRichDateTime(QPainter * painter, const QStyleOptionViewItem & option, int dateTimeSecs) const;
    QSize defaultSizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const;
    QSize sizeHintForText(const QStyleOptionViewItem & option, const QString& textData) const;
private:
    int m_defaultSectionHeight;
    QSize m_playBottonSize;
    mutable int m_widthHint;
    mutable QHash<QString, int> m_sizeHintHash;
};



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
    void at_sessionsGrid_clicked(const QModelIndex & index);
    void at_sessionsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_selectAllAction_triggered();
    void at_masterItemPressed(const QModelIndex& index);
    void at_ItemPressed(const QModelIndex& index);
    void at_ItemEntered(const QModelIndex& index);
    void at_eventsGrid_clicked(const QModelIndex& idx);
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

    QnAuditRecordRefList filterChildDataBySessions(const QnAuditRecordRefList& checkedRows);
    QnAuditRecordRefList filterChildDataByCameras(const QnAuditRecordRefList& checkedRows);
    void setupFilterCheckbox(QCheckBox* checkbox, const QColor& color, Qn::AuditRecordTypes filteredTypes);
    void processPlaybackAction(const QnAuditRecord* record);
    void triggerAction(const QnAuditRecord* record, Qn::ActionId ActionId, const QString& objectName);
    QnAuditRecordRefList applyFilter();
    QSize calcButtonSize(const QFont& font) const;
    void setupSessionsGrid();
    void setupCamerasGrid();
    void setupMasterGridCommon(QnTableView* gridMaster);
    void makeSessionData();
    void makeCameraData();
    void setupContextMenu(QTableView* gridMaster);
    QTableView* currentGridView() const;
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
    //QnCheckBoxedHeaderView* m_masterHeaders;
    //QnCheckBoxedHeaderView* m_cameraHeaders;
    QList<QCheckBox*> m_filterCheckboxes;
    QModelIndex m_hoveredIndex;
    
    bool m_skipNextPressSignal;
    QModelIndex m_skipNextSelIndex;
    QModelIndex m_lastPressIndex;
    QnAuditItemDelegate* m_itemDelegate;
};

#endif // QN_AUDIT_LOG_DIALOG_H

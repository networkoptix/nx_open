#ifndef QN_AUDIT_LOG_DIALOG_H
#define QN_AUDIT_LOG_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "api/model/audit/audit_record.h"

class QnAuditLogSessionModel;
class QnAuditLogDetailModel;

namespace Ui {
    class AuditLogDialog;
}

class QnAuditDetailItemDelegate: public QStyledItemDelegate 
{
    typedef QStyledItemDelegate base_type;

public:
    explicit QnAuditDetailItemDelegate(QObject *parent = NULL): base_type(parent) {}
    virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;

    virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;
    void setButtonExtraSize(const QSize& value) { m_btnSize = value; }

private:
    QSize m_btnSize;
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
    void at_mouseButtonRelease(QObject* sender, QEvent* event);
    void at_ItemPressed(const QModelIndex& index);
    void at_eventsGrid_clicked(const QModelIndex& idx);
private:
    QList<QnMediaServerResourcePtr> getServerList() const;
    void requestFinished();

    /**
     * Get data from server
     * 
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 fromMsec, qint64 toMsec);

    QnAuditRecordList filteredChildData(const QModelIndexList& selection);
private:
    QScopedPointer<Ui::AuditLogDialog> ui;

    QnAuditLogSessionModel *m_sessionModel;
    QnAuditLogDetailModel *m_detailModel;
    QSet<int> m_requests;

    QnAuditRecordList m_allData;
    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
};

#endif // QN_AUDIT_LOG_DIALOG_H

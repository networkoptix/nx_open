#ifndef QN_RECORDING_STATISTICS_DIALOG_H
#define QN_RECORDING_STATISTICS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include "api/model/recording_stats_reply.h"

class QnRecordingStatsModel;

namespace Ui {
    class RecordingStatsDialog;
}

class QnRecordingStatsDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    explicit QnRecordingStatsDialog(QWidget *parent);
    virtual ~QnRecordingStatsDialog();

    void disableUpdateData();
    void enableUpdateData();
    void setDateRange(const QDate& from, const QDate& to);

protected:
    void setVisible(bool value) override;

private slots:
    void updateData();
    void at_gotStatiscits(int httpStatus, const QnRecordingStatsReply& data, int requestNum);
    void at_eventsGrid_clicked(const QModelIndex & index);
    void at_eventsGrid_customContextMenuRequested(const QPoint& screenPos);
    void at_clipboardAction_triggered();
    void at_exportAction_triggered();
    void at_mouseButtonRelease(QObject* sender, QEvent* event);

private:
    void updateHeaderWidth();
    void requestFinished();
    QList<QnMediaServerResourcePtr> getServerList() const;

    /**
     * Get data from server
     * 
     * \param fromMsec start date. UTC msecs
     * \param toMsec end date. UTC msecs. Can be DATETIME_NOW
     */
    void query(qint64 fromMsec, qint64 toMsec);

private:
    QScopedPointer<Ui::RecordingStatsDialog> ui;
    QnRecordingStatsModel *m_model;

    QSet<int> m_requests;

    bool m_updateDisabled;
    bool m_dirty;

    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    Qt::MouseButton m_lastMouseButton;
    QnRecordingStatsReply m_allData;
};

#endif // QN_RECORDING_STATISTICS_DIALOG_H

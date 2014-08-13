/**********************************************************
* 08 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef TIME_SERVER_SELECTION_DIALOG_H
#define TIME_SERVER_SELECTION_DIALOG_H

#include <memory>

#include <QDialog>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/id.h>


namespace Ui {
    class TimeServerSelectionDialog;
}

class QnTimeServerSelectionDialog
:
    public QDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QDialog base_type;

public:
    QnTimeServerSelectionDialog( QWidget* parent = NULL, QnWorkbenchContext* context = NULL );
    virtual ~QnTimeServerSelectionDialog();

    void setData(
        qint64 localSystemTime,
        const QList<QPair<QUuid, qint64> >& peersAndTimes );
    QUuid selectedPeer() const;

public slots:
    virtual void accept() override;

protected:
    virtual void showEvent( QShowEvent* ) override;
    virtual void hideEvent( QHideEvent* ) override;

private:
    std::unique_ptr<Ui::TimeServerSelectionDialog> m_ui;
    qint64 m_timeBase;
    QTimer m_timer;
    //list<pair<peerid, local time on peer at \a m_timeBase time point>>. All times are UTC, millis from epoch
    QList<QPair<QUuid, qint64> > m_peersAndTimes;

private slots:
    void onTimer();
    void onTableItemClicked( QTableWidgetItem* item );
};

#endif  //TIME_SERVER_SELECTION_DIALOG_H

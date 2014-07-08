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
        const QList<QPair<QnId, qint64> >& peersAndTimes );

public slots:
    virtual void accept() override;

private:
    std::unique_ptr<Ui::TimeServerSelectionDialog> m_ui;
};

#endif  //TIME_SERVER_SELECTION_DIALOG_H

#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnSearchBookmarksDialogPrivate;

class QnSearchBookmarksDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnSearchBookmarksDialog(const QString &filterText
        , qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , QWidget *parent = nullptr);

    virtual ~QnSearchBookmarksDialog();

    void setParameters(qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , const QString &filterText = QString());

private:
    void resizeEvent(QResizeEvent *event);

    void showEvent(QShowEvent *event);

private:
    Q_DECLARE_PRIVATE(QnSearchBookmarksDialog)
    QScopedPointer<QnSearchBookmarksDialogPrivate> d_ptr;
};

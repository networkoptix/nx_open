
#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnSearchBookmarksDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
public:
    QnSearchBookmarksDialog(QWidget *parent = nullptr);

    virtual ~QnSearchBookmarksDialog();

    void setParameters(qint64 utcStartTimeMs
        , qint64 utcFinishTimeMs
        , const QString &filterText = QString());

private:
    void resizeEvent(QResizeEvent *event);

    void showEvent(QShowEvent *event);

private:
    typedef QnButtonBoxDialog Base;
    class Impl;

    Impl * const m_impl;
};
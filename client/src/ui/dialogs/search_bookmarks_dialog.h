
#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnSearchBookmarksDialog: public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

public:
    QnSearchBookmarksDialog(QWidget *parent = nullptr);

    virtual ~QnSearchBookmarksDialog();

private:
    void resizeEvent(QResizeEvent *event);

private:
    typedef QnWorkbenchStateDependentButtonBoxDialog Base;
    class Impl;

    Impl * const m_impl;
};

#pragma once

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QnSearchBookmarksDialog: public QnButtonBoxDialog
{
public:
    QnSearchBookmarksDialog(QWidget *parent = nullptr);

    virtual ~QnSearchBookmarksDialog();

    void setParameters(const QString &filterText
        , const QDate &start
        , const QDate &finish);

private:
    void resizeEvent(QResizeEvent *event);

    void showEvent(QShowEvent *event);

private:
    typedef QnButtonBoxDialog Base;
    class Impl;

    Impl * const m_impl;
};
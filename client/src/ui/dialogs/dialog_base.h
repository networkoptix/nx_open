
#pragma once

#include <QDialog>

class QnDialogBase : public QDialog
{
public:
    QnDialogBase(QWidget * parent = nullptr, Qt::WindowFlags flags = 0);

    static void show(QnDialogBase *dialog);

    void show();

private:
    void cancelDrag();
};

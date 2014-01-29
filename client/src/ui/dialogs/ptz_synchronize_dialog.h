#ifndef PTZ_SYNCHRONIZE_DIALOG_H
#define PTZ_SYNCHRONIZE_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnPtzSynchronizeDialog;
}

class QnPtzSynchronizeDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnPtzSynchronizeDialog(QWidget *parent = 0);
    ~QnPtzSynchronizeDialog();
private:
    QScopedPointer<Ui::QnPtzSynchronizeDialog> ui;
};

#endif // PTZ_SYNCHRONIZE_DIALOG_H

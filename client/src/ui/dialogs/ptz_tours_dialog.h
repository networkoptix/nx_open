#ifndef PTZ_TOURS_DIALOG_H
#define PTZ_TOURS_DIALOG_H

#include <QtWidgets/QDialog>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnPtzToursDialog;
}

class QnPtzToursDialog : public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnPtzToursDialog(QWidget *parent = 0);
    ~QnPtzToursDialog();

private:
    QScopedPointer<Ui::QnPtzToursDialog> ui;
};

#endif // PTZ_TOURS_DIALOG_H

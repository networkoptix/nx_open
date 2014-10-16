#ifndef BUILD_NUMBER_DIALOG_H
#define BUILD_NUMBER_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnBuildNumberDialog;
}

class QnBuildNumberDialog : public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnBuildNumberDialog(QWidget *parent = 0);
    ~QnBuildNumberDialog();

    int buildNumber() const;
    QString password() const;

    virtual void accept() override;

private:
    QScopedPointer<Ui::QnBuildNumberDialog> ui;
};

#endif // BUILD_NUMBER_DIALOG_H

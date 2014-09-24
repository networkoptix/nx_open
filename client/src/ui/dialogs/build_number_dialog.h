#ifndef BUILD_NUMBER_DIALOG_H
#define BUILD_NUMBER_DIALOG_H

#include <QDialog>

namespace Ui {
class QnBuildNumberDialog;
}

class QnBuildNumberDialog : public QDialog {
    Q_OBJECT

    typedef QDialog base_type;
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

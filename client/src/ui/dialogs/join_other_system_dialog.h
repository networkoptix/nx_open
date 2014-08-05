#ifndef JOIN_OTHER_SYSTEM_DIALOG_H
#define JOIN_OTHER_SYSTEM_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
class QnJoinOtherSystemDialog;
}

class QnJoinOtherSystemDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnJoinOtherSystemDialog(QWidget *parent = 0);
    ~QnJoinOtherSystemDialog();

    QUrl url() const;
    QString password() const;
    void updateUi();

private slots:
    void at_urlComboBox_activated(int index);

private:
    QScopedPointer<Ui::QnJoinOtherSystemDialog> ui;
};

#endif // JOIN_OTHER_SYSTEM_DIALOG_H

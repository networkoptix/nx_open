#ifndef UPDATE_URL_DIALOG_H
#define UPDATE_URL_DIALOG_H

#include <QtWidgets/QDialog>

namespace Ui {
class QnUpdateUrlDialog;
}

class QnUpdateUrlDialog : public QDialog {
    Q_OBJECT

public:
    explicit QnUpdateUrlDialog(QWidget *parent = 0);
    ~QnUpdateUrlDialog();

    QString updatesUrl() const;
    void setUpdatesUrl(const QString &url);

private slots:
    void at_copyButton_clicked();

private:
    QScopedPointer<Ui::QnUpdateUrlDialog> ui;
};

#endif // UPDATE_URL_DIALOG_H

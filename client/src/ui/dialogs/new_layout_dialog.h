#ifndef QN_NEW_LAYOUT_DIALOG_H
#define QN_NEW_LAYOUT_DIALOG_H

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
    class NewLayoutDialog;
}

class QnNewLayoutDialog: public QDialog {
    Q_OBJECT;
public:
    QnNewLayoutDialog(QWidget *parent = NULL);

    virtual ~QnNewLayoutDialog();

    QString name();

    void setName(const QString &name);

protected slots:
    void at_nameLineEdit_textChanged(const QString& text);

private:
    QScopedPointer<Ui::NewLayoutDialog> ui;
};

#endif // QN_NEW_LAYOUT_DIALOG_H

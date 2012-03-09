#ifndef QN_LAYOUT_NAME_DIALOG_H
#define QN_LAYOUT_NAME_DIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QDialogButtonBox>

#include "button_box_dialog.h"

namespace Ui {
    class LayoutNameDialog;
}

class QnLayoutNameDialog: public QnButtonBoxDialog {
    Q_OBJECT;
public:
    QnLayoutNameDialog(const QString &caption, const QString &text, const QString &name, QDialogButtonBox::StandardButtons buttons, QWidget *parent = NULL);
    QnLayoutNameDialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent = NULL);

    virtual ~QnLayoutNameDialog();

    QString name() const;

    void setName(const QString &name);

    QString text() const;

    void setText(const QString &label);

protected slots:
    void at_nameLineEdit_textChanged(const QString &text);

private:
    void init(QDialogButtonBox::StandardButtons buttons);

private:
    QScopedPointer<Ui::LayoutNameDialog> ui;
};

#endif // QN_LAYOUT_NAME_DIALOG_H

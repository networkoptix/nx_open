#pragma once

#include <QtWidgets/QDialog>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace Ui {
    class LayoutNameDialog;
}

class QnLayoutNameDialog: public QnSessionAwareButtonBoxDialog {
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    QnLayoutNameDialog(const QString &caption, const QString &text, const QString &name, QDialogButtonBox::StandardButtons buttons, QWidget *parent);
    QnLayoutNameDialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent);

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

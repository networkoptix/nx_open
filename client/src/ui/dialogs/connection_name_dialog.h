#ifndef QN_LAYOUT_NAME_DIALOG_H
#define QN_LAYOUT_NAME_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
    class ConnectionNameDialog;
}

class QnConnectionNameDialog: public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    QnConnectionNameDialog(QWidget *parent = NULL);
    virtual ~QnConnectionNameDialog();

    QString name() const;
    void setName(const QString &name);

    bool savePassword() const;
    void setSavePassword(bool savePassword);

    void setSavePasswordEnabled(bool enabled);

protected slots:
    void at_nameLineEdit_textChanged(const QString &text);

private:
    QScopedPointer<Ui::ConnectionNameDialog> ui;
};

#endif // QN_LAYOUT_NAME_DIALOG_H

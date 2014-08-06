#ifndef QN_LAYOUT_NAME_DIALOG_H
#define QN_LAYOUT_NAME_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtCore/QScopedPointer>
#include <QtWidgets/QDialogButtonBox>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class LayoutNameDialog;
}

class QnLayoutNameDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
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

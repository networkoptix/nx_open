#ifndef QN_LAYOUT_NAME_DIALOG_H
#define QN_LAYOUT_NAME_DIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QDialogButtonBox>

namespace Ui {
    class LayoutNameDialog;
}

class QnLayoutNameDialog: public QDialog {
    Q_OBJECT;
public:
    QnLayoutNameDialog(const QString &caption, const QString &text, const QString &name, QDialogButtonBox::StandardButtons buttons, QWidget *parent = NULL);
    QnLayoutNameDialog(QDialogButtonBox::StandardButtons buttons, QWidget *parent = NULL);

    virtual ~QnLayoutNameDialog();

    QString name() const;

    void setName(const QString &name);

    QString text() const;

    void setText(const QString &label);

    QDialogButtonBox::StandardButton clickedButton() const {
        return m_clickedButton;
    }

protected slots:
    void at_nameLineEdit_textChanged(const QString &text);
    void at_buttonBox_clicked(QAbstractButton *button);

private:
    void init(QDialogButtonBox::StandardButtons buttons);

private:
    QScopedPointer<Ui::LayoutNameDialog> ui;
    QDialogButtonBox::StandardButton m_clickedButton;
};

#endif // QN_LAYOUT_NAME_DIALOG_H

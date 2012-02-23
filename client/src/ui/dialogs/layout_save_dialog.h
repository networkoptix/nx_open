#ifndef QN_LAYOUT_SAVE_DIALOG_H
#define QN_LAYOUT_SAVE_DIALOG_H

#include <QDialog>
#include <QScopedPointer>
#include <QDialogButtonBox>

class QAbstractButton;

namespace Ui {
    class LayoutSaveDialog;
}

class QnLayoutSaveDialog: public QDialog {
    Q_OBJECT;
public:
    QnLayoutSaveDialog(QWidget *parent = NULL);

    virtual ~QnLayoutSaveDialog();

    QString layoutName() const;

    void setLayoutName(const QString &layoutName);

    QDialogButtonBox::StandardButton clickedButton() const {
        return m_clickedButton;
    }

protected slots:
    void at_buttonBox_clicked(QAbstractButton *button);

private:
    QScopedPointer<Ui::LayoutSaveDialog> ui;
    QString m_labelText;
    QDialogButtonBox::StandardButton m_clickedButton;
};


#endif // QN_LAYOUT_SAVE_DIALOG_H

#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QtCore/QScopedPointer>

#include <QtGui/QDialog>

namespace Ui {
    class AboutDialog;
}

class QnAboutDialog : public QDialog {
    Q_OBJECT;

public:
    explicit QnAboutDialog(QWidget *parent = 0);
    virtual ~QnAboutDialog();

protected:
    virtual void changeEvent(QEvent *event) override;

protected slots:
    void at_copyButton_clicked();

private:
    void retranslateUi();

private:
    QScopedPointer<Ui::AboutDialog> ui;
    QPushButton *m_copyButton;
};

#endif // ABOUTDIALOG_H

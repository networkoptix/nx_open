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

private:
    void retranslateUi();

private:
    QScopedPointer<Ui::AboutDialog> ui;
};

#endif // ABOUTDIALOG_H

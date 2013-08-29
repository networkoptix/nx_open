#ifndef QN_ABOUT_DIALOG_H
#define QN_ABOUT_DIALOG_H

#include <QtCore/QScopedPointer>

#include <ui/dialogs/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class AboutDialog;
}

class QnAboutDialog : public QnButtonBoxDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
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

#endif // QN_ABOUT_DIALOG_H

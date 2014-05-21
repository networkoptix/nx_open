#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QDialog>

namespace Ui {
class QnUpdateDialog;
}

class QnServerUpdatesWidget;

class QnUpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit QnUpdateDialog(QWidget *parent = 0);
    ~QnUpdateDialog();

private:
    QScopedPointer<Ui::QnUpdateDialog> ui;
    QnServerUpdatesWidget *m_updatesWidget;
};

#endif // UPDATE_DIALOG_H

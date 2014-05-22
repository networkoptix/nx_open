#ifndef UPDATE_DIALOG_H
#define UPDATE_DIALOG_H

#include <QtWidgets/QDialog>

#include <utils/common/id.h>

namespace Ui {
class QnUpdateDialog;
}

class QnServerUpdatesWidget;
class QnMediaServerUpdateTool;

class QnUpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit QnUpdateDialog(QWidget *parent = 0);
    ~QnUpdateDialog();

    QnMediaServerUpdateTool *updateTool() const;

private:
    QScopedPointer<Ui::QnUpdateDialog> ui;
    QnServerUpdatesWidget *m_updatesWidget;
};

#endif // UPDATE_DIALOG_H

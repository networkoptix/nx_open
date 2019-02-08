#ifndef BUILD_NUMBER_DIALOG_H
#define BUILD_NUMBER_DIALOG_H

#include <ui/dialogs/common/button_box_dialog.h>

namespace Ui {
class QnBuildNumberDialog;
}

class QnBuildNumberDialog : public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit QnBuildNumberDialog(QWidget* parent);
    virtual ~QnBuildNumberDialog();

    int buildNumber() const;
    QString changeset() const;
    QString password() const;

    virtual void accept() override;

private:
    QScopedPointer<Ui::QnBuildNumberDialog> ui;
};

#endif // BUILD_NUMBER_DIALOG_H

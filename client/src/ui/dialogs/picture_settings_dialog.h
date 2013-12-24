#ifndef PICTURE_SETTINGS_DIALOG_H
#define PICTURE_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnPictureSettingsDialog;
}

class QnPictureSettingsDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnPictureSettingsDialog(QWidget *parent = 0);
    ~QnPictureSettingsDialog();

    void updateFromResource(const QnMediaResourcePtr &resource);
    void submitToResource(const QnMediaResourcePtr &resource);

private slots:
    void at_fisheyeCheckbox_toggled(bool checked);

private:
    QScopedPointer<Ui::QnPictureSettingsDialog> ui;
};

#endif // PICTURE_SETTINGS_DIALOG_H

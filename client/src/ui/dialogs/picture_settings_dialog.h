#ifndef PICTURE_SETTINGS_DIALOG_H
#define PICTURE_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnPictureSettingsDialog;
}

class QnPictureSettingsDialog : public QnButtonBoxDialog, public QnWorkbenchContextAware
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

    void paramsChanged();
private:
    QScopedPointer<Ui::QnPictureSettingsDialog> ui;

    bool m_updating;
    QnMediaResourcePtr m_resource;
};

#endif // PICTURE_SETTINGS_DIALOG_H

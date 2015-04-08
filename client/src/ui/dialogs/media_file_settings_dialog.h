#ifndef PICTURE_SETTINGS_DIALOG_H
#define PICTURE_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class QnMediaFileSettingsDialog;
}

class QnImageProvider;

class QnMediaFileSettingsDialog : public QnButtonBoxDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnMediaFileSettingsDialog(QWidget *parent = 0);
    ~QnMediaFileSettingsDialog();

    void updateFromResource(const QnMediaResourcePtr &resource);
    void submitToResource(const QnMediaResourcePtr &resource);

private slots:
    void at_fisheyeCheckbox_toggled(bool checked);

    void paramsChanged();
private:
    QScopedPointer<Ui::QnMediaFileSettingsDialog> ui;

    bool m_updating;
    QnMediaResourcePtr m_resource;
    QnImageProvider* m_imageProvider;
};

#endif // PICTURE_SETTINGS_DIALOG_H

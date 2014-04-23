#ifndef VIDEOWALL_SETTINGS_DIALOG_H
#define VIDEOWALL_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/button_box_dialog.h>

namespace Ui {
class QnVideowallSettingsDialog;
}

class QnVideowallSettingsDialog : public QnButtonBoxDialog
{
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnVideowallSettingsDialog(QWidget *parent = 0);
    ~QnVideowallSettingsDialog();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall) const;
private:
    QScopedPointer<Ui::QnVideowallSettingsDialog> ui;
};

#endif // VIDEOWALL_SETTINGS_DIALOG_H

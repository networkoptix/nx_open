#ifndef VIDEOWALL_SETTINGS_DIALOG_H
#define VIDEOWALL_SETTINGS_DIALOG_H

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
class QnVideowallSettingsDialog;
}

class QnVideowallSettingsDialog : public QnWorkbenchStateDependentButtonBoxDialog
{
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    explicit QnVideowallSettingsDialog(QWidget *parent = 0);
    ~QnVideowallSettingsDialog();

    void loadFromResource(const QnVideoWallResourcePtr &videowall);
    void submitToResource(const QnVideoWallResourcePtr &videowall);

private:
    static QString shortcutPath();
    bool shortcutExists(const QnVideoWallResourcePtr &videowall) const;
    bool createShortcut(const QnVideoWallResourcePtr &videowall);
    bool deleteShortcut(const QnVideoWallResourcePtr &videowall);

    bool canStartVideowall(const QnVideoWallResourcePtr &videowall) const;
private:
    QScopedPointer<Ui::QnVideowallSettingsDialog> ui;
};

#endif // VIDEOWALL_SETTINGS_DIALOG_H

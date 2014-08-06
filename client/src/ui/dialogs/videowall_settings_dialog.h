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
    void submitToResource(const QnVideoWallResourcePtr &videowall) const;

    bool isShortcutsSupported() const;
    void setShortcutsSupported(bool value);

    bool isCreateShortcut() const;
    void setCreateShortcut(bool value);
private:
    QScopedPointer<Ui::QnVideowallSettingsDialog> ui;
};

#endif // VIDEOWALL_SETTINGS_DIALOG_H

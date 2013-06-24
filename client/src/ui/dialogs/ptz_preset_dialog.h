#ifndef QN_PTZ_PRESET_DIALOG_H
#define QN_PTZ_PRESET_DIALOG_H

#include "button_box_dialog.h"

#include <ui/workbench/workbench_ptz_preset_manager.h>

namespace Ui {
    class PtzPresetDialog;
}

class QnPtzPresetDialog: public QnButtonBoxDialog {
    Q_OBJECT;
    typedef QnButtonBoxDialog base_type;

public:
    QnPtzPresetDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetDialog();

    QnPtzPreset preset() const;
    void setPreset(const QnPtzPreset &preset);

    const QList<int> &hotkeys() const;
    void setHotkeys(const QList<int> &hotkeys);

protected:
    int currentHotkey() const;
    void setCurrentHotkey(int hotkey);

private:
    QScopedPointer<Ui::PtzPresetDialog> ui;
    QnPtzPreset m_preset;
    QList<int> m_hotkeys;
};

#endif // QN_PTZ_PRESET_DIALOG_H
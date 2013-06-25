#ifndef QN_PTZ_PRESET_DIALOG_H
#define QN_PTZ_PRESET_DIALOG_H

#include "button_box_dialog.h"

#include <ui/workbench/workbench_ptz_preset_manager.h>

namespace Ui {
    class PtzPresetDialog;
}

class QnPtzPresetDialog: public QnButtonBoxDialog {
    Q_OBJECT
    typedef QnButtonBoxDialog base_type;

public:
    QnPtzPresetDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetDialog();

    QnPtzPreset preset() const;
    void setPreset(const QnPtzPreset &preset);

    const QList<QKeySequence> &forbiddenHotkeys() const;
    void setForbiddenHotkeys(const QList<QKeySequence> &forbiddenHotkeys);

protected:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

    Q_SLOT void updateOkButtonEnabled();
    void updateHotkey();

    bool isHotkeyValid() const;
private:
    QScopedPointer<Ui::PtzPresetDialog> ui;
    QnPtzPreset m_preset;
    QList<QKeySequence> m_forbiddenHotkeys;

    int m_key;
    Qt::KeyboardModifiers m_modifiers;
};

#endif // QN_PTZ_PRESET_DIALOG_H

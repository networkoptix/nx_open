#ifndef QN_PTZ_PRESET_DIALOG_H
#define QN_PTZ_PRESET_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

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

    QString name() const;
    void setName(const QString &name);

    int hotkey() const;
    void setHotkey(int hotkey);

    const QList<int> &forbiddenHotkeys() const;
    void setForbiddenHotkeys(const QList<int> &forbiddenHotkeys);

protected:
    void setForbiddenHotkeys(const QList<int> &forbiddenHotkeys, bool force);

    Q_SLOT void updateOkButtonEnabled();

private:
    QScopedPointer<Ui::PtzPresetDialog> ui;
    QList<int> m_forbiddenHotkeys;
};

#endif // QN_PTZ_PRESET_DIALOG_H

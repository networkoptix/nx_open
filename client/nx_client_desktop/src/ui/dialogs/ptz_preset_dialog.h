#ifndef QN_PTZ_PRESET_DIALOG_H
#define QN_PTZ_PRESET_DIALOG_H

#include <core/ptz/ptz_fwd.h>

#include <ui/dialogs/abstract_ptz_dialog.h>

namespace Ui {
    class PtzPresetDialog;
}


class QnPtzPresetDialog: public QnAbstractPtzDialog {
    Q_OBJECT

    typedef QnAbstractPtzDialog base_type;
public:
    QnPtzPresetDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetDialog();

    QnAbstractPtzHotkeyDelegate* hotkeysDelegate() const;
    void setHotkeysDelegate(QnAbstractPtzHotkeyDelegate *delegate);

protected:
    virtual void loadData(const QnPtzData &data) override;
    virtual void saveData() override;
    virtual Qn::PtzDataFields requiredFields() const override;
    virtual void updateFields(Qn::PtzDataFields fields) override;

protected:
    int hotkey() const;
    void setHotkey(int hotkey);

    Q_SLOT void updateOkButtonEnabled();
private:
    QScopedPointer<Ui::PtzPresetDialog> ui;
    QnAbstractPtzHotkeyDelegate* m_hotkeysDelegate;
};

#endif // QN_PTZ_PRESET_DIALOG_H

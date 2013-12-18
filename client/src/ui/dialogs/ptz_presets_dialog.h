#ifndef QN_PTZ_PRESETS_DIALOG_H
#define QN_PTZ_PRESETS_DIALOG_H

#include <QtWidgets/QDialog>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_hotkey.h>

#include <ui/dialogs/abstract_ptz_dialog.h>

class QPushButton;
class QnPtzPresetListModel;

namespace Ui {
    class PtzPresetsDialog;
}

class QnPtzPresetsDialog: public QnAbstractPtzDialog {
    Q_OBJECT

    typedef QnAbstractPtzDialog base_type;

public:
    QnPtzPresetsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetsDialog();

    QnAbstractPtzHotkeyDelegate* hotkeysDelegate() const;
    void setHotkeysDelegate(QnAbstractPtzHotkeyDelegate *delegate);

protected:
    virtual void loadData(const QnPtzData &data) override;
    virtual void saveData() const override;
    virtual Qn::PtzDataFields requiredFields() const override;

private slots:
    void at_removeButton_clicked();
    void at_activateButton_clicked();

    void updateRemoveButtonEnabled();
    void updateActivateButtonEnabled();
private:
    QScopedPointer<Ui::PtzPresetsDialog> ui;

    QPushButton *m_removeButton;
    QPushButton *m_activateButton;
    QnPtzPresetListModel *m_model;
    QnPtzPresetList m_oldPresets;
    QnAbstractPtzHotkeyDelegate* m_hotkeysDelegate;
};

#endif // QN_PTZ_PRESETS_DIALOG_H

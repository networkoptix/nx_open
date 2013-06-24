#ifndef QN_PTZ_PRESET_DIALOG_H
#define QN_PTZ_PRESET_DIALOG_H

#include "button_box_dialog.h"

namespace Ui {
    class PtzPresetDialog;
}

class QnPtzPresetDialog: public QnButtonBoxDialog {
    Q_OBJECT;
    typedef QnButtonBoxDialog base_type;

public:
    QnPtzPresetDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetDialog();

private:
    QScopedPointer<Ui::PtzPresetDialog> ui;
};

#endif // QN_PTZ_PRESET_DIALOG_H
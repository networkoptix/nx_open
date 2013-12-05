#ifndef QN_PTZ_PRESETS_DIALOG_H
#define QN_PTZ_PRESETS_DIALOG_H

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_ptz_preset_manager.h>

#include "button_box_dialog.h"

class QPushButton;
class QnPtzPresetListModel;

namespace Ui {
    class PtzPresetsDialog;
}

class QnPtzPresetsDialog: public Connective<QnButtonBoxDialog>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QnButtonBoxDialog> base_type;

public:
    QnPtzPresetsDialog(QWidget *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnPtzPresetsDialog();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    virtual void accept() override;

private slots:
    void at_removeButton_clicked();
    void at_activateButton_clicked();

    void updateFromResource();
    void submitToResource();

    void updateLabel();
    void updateModel();
    void updateRemoveButtonEnabled();
    void updateActivateButtonEnabled();

private:
    QScopedPointer<Ui::PtzPresetsDialog> ui;
    QnVirtualCameraResourcePtr m_camera;
    QPushButton *m_removeButton;
    QPushButton *m_activateButton;
    //QnPtzPresetListModel *m_model;
};

#endif // QN_PTZ_PRESETS_DIALOG_H

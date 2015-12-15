#pragma once

#include <ui/dialogs/resource_selection_dialog.h>

class QnBackupCamerasDialog: public QnResourceSelectionDialog {
    Q_OBJECT

    typedef QnResourceSelectionDialog base_type;
public:
    QnBackupCamerasDialog(QWidget* parent = nullptr);

    static QString qualitiesToString(Qn::CameraBackupQualities qualities);

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    void updateQualitiesForSelectedCameras(Qn::CameraBackupQualities qualities);
};

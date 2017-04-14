#pragma once

#include <ui/dialogs/resource_selection_dialog.h>

class QnBackupCamerasDialog: public QnResourceSelectionDialog {
    Q_OBJECT

    typedef QnResourceSelectionDialog base_type;
public:
    QnBackupCamerasDialog(QWidget* parent = nullptr);

    bool backupNewCameras() const;
    void setBackupNewCameras(bool value);
};

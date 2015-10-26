#pragma once

#include <client/client_color_types.h>

#include <ui/models/resource_pool_model.h>

#include <ui/dialogs/resource_selection_dialog.h>

class QnBackupCamerasResourceModelDelegate;

class QnBackupCamerasDialog: public QnResourceSelectionDialog {
    Q_OBJECT
    
    typedef QnResourceSelectionDialog base_type;

    Q_PROPERTY(QnBackupCamerasColors colors READ colors WRITE setColors)
public:
    QnBackupCamerasDialog(QWidget* parent = nullptr);

    static QString qualitiesToString(Qn::CameraBackupQualities qualities);

    const QnBackupCamerasColors &colors() const;
    void setColors(const QnBackupCamerasColors &colors);

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

private:
    void updateQualitiesForSelectedCameras(Qn::CameraBackupQualities qualities);

private:
    class QnBackupCamerasDialogDelegate;

    QnBackupCamerasDialogDelegate* m_delegate;
    QnBackupCamerasResourceModelDelegate* m_customColumnDelegate;
};

#ifndef RESOURCE_TREE_DIALOG_H
#define RESOURCE_TREE_DIALOG_H

#include <QDialog>

#include "core/resource/resource_fwd.h"

#include <ui/workbench/workbench_context_aware.h>


namespace Ui {
    class QnExportCameraSettingsDialog;
}

class QnResourcePoolModel;

class QnExportCameraSettingsDialog : public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
    
    typedef QDialog base_type;

public:
    explicit QnExportCameraSettingsDialog(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnExportCameraSettingsDialog();

    QnVirtualCameraResourceList getSelectedCameras() const;

    void setRecordingEnabled(bool enabled = true);

    void setMotionParams(bool motionUsed, bool dualStreamingUsed);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

private slots:
    void at_resourceModel_dataChanged();

    void updateLicensesStatus();
    void updateMotionStatus();
    void updateDtsStatus();
    void updateOkStatus();

private:
    QScopedPointer<Ui::QnExportCameraSettingsDialog> ui;

    QnResourcePoolModel *m_resourceModel;

    bool m_recordingEnabled;
    bool m_motionUsed;
    bool m_dualStreamingUsed;

    bool m_licensesOk;
    bool m_motionOk;
    bool m_dtsOk;
};

#endif // RESOURCE_TREE_DIALOG_H

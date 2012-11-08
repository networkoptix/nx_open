#ifndef RESOURCE_TREE_DIALOG_H
#define RESOURCE_TREE_DIALOG_H

#include <QDialog>

#include "core/resource/resource_fwd.h"

#include <ui/workbench/workbench_context_aware.h>


namespace Ui {
    class QnResourceTreeDialog;
}

class QnResourcePoolModel;

class QnResourceTreeDialog : public QDialog, public QnWorkbenchContextAware
{
    Q_OBJECT
    
public:
    explicit QnResourceTreeDialog(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnResourceTreeDialog();

    QnVirtualCameraResourceList getSelectedCameras() const;

    void setRecordingEnabled(bool enabled = true);

    void setMotionParams(bool motionUsed, bool dualStreamingUsed);
private slots:
    void at_resourceModel_dataChanged();

    void updateLicensesStatus();
    void updateMotionStatus();
    void updateOkStatus();
private:
    QScopedPointer<Ui::QnResourceTreeDialog> ui;

    QnResourcePoolModel *m_resourceModel;

    bool m_recordingEnabled;
    bool m_motionUsed;
    bool m_dualStreamingUsed;

    bool m_licensesOk;
    bool m_motionOk;
};

#endif // RESOURCE_TREE_DIALOG_H

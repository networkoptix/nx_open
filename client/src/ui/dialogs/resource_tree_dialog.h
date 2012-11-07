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
private slots:
    void at_resourceModel_dataChanged();

    void updateLicensesLabelText();
private:
    QScopedPointer<Ui::QnResourceTreeDialog> ui;

    QnResourcePoolModel *m_resourceModel;

    bool m_recordingEnabled;
};

#endif // RESOURCE_TREE_DIALOG_H

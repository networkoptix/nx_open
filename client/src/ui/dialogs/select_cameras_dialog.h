#ifndef SELECT_CAMERAS_DIALOG_H
#define SELECT_CAMERAS_DIALOG_H

#include <QDialog>

#include "core/resource/resource_fwd.h"

#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class QnSelectCamerasDialog;
}

class QnResourcePoolModel;

class QnSelectCamerasDialog : public QDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QDialog base_type;
    
public:
    explicit QnSelectCamerasDialog(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    ~QnSelectCamerasDialog();

    QnResourceList getSelectedResources() const;
    void setSelectedResources(const QnResourceList &selected);
protected:
    virtual void keyPressEvent(QKeyEvent *event) override;
private slots:
    void at_resourceModel_dataChanged(QModelIndex old, QModelIndex upd);

private:
    Ui::QnSelectCamerasDialog *ui;

    QnResourcePoolModel *m_resourceModel;
};

#endif // SELECT_CAMERAS_DIALOG_H

#ifndef SELECT_CAMERAS_DIALOG_H
#define SELECT_CAMERAS_DIALOG_H

#include <QtCore/QModelIndex>

#include <QtWidgets/QDialog>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/workbench/workbench_context_aware.h>
#include "utils/common/id.h"

#include <ui/dialogs/workbench_state_dependent_dialog.h>

namespace Ui {
    class ResourceSelectionDialog;
}

class QnResourcePoolModel;
class QnCameraThumbnailManager;

class QnResourceSelectionDialog : public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT
    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;

public:
    enum SelectionTarget {
        CameraResourceTarget,
        UserResourceTarget
    };

    explicit QnResourceSelectionDialog(SelectionTarget target, QWidget *parent = NULL);
    explicit QnResourceSelectionDialog(QWidget *parent = NULL);
    ~QnResourceSelectionDialog();

    QnResourceList selectedResources() const;
    void setSelectedResources(const QnResourceList &selected);

    QnResourceSelectionDialogDelegate *delegate() const;
    void setDelegate(QnResourceSelectionDialogDelegate* delegate);

protected:
    virtual void keyPressEvent(QKeyEvent *event) override;

    QnResourceList selectedResourcesInner(const QModelIndex &parent = QModelIndex()) const;
    int setSelectedResourcesInner(const QnResourceList &selected, const QModelIndex &parent = QModelIndex());

private slots:
    void at_resourceModel_dataChanged();
    void at_thumbnailReady(QnUuid resourceId, const QPixmap &thumbnail);

    QModelIndex itemIndexAt(const QPoint &pos) const;
    void updateThumbnail(const QModelIndex &index);
private:
    void init();
private:
    QScopedPointer<Ui::ResourceSelectionDialog> ui;
    QnResourcePoolModel *m_resourceModel;
    QnResourceSelectionDialogDelegate* m_delegate;
    QnCameraThumbnailManager *m_thumbnailManager;
    SelectionTarget m_target;
    QnUuid m_tooltipResourceId;

    int m_screenshotIndex;

    bool m_updating;
};

#endif // SELECT_CAMERAS_DIALOG_H

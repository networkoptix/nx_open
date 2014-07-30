#ifndef QN_CAMERA_LIST_DIALOG_H
#define QN_CAMERA_LIST_DIALOG_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtWidgets/QDialog>
#include <QtGui/QStandardItem>

#include <ui/workbench/workbench_context_aware.h>
#include <core/resource/resource_fwd.h>

class QnCameraListModel;
class QnWorkbenchContext;
class QnResourceSearchProxyModel;

namespace Ui {
    class CameraListDialog;
}

class QnCameraListDialog: public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QDialog base_type;

public:
    explicit QnCameraListDialog(QWidget *parent = NULL);
    virtual ~QnCameraListDialog();

    void setServer(const QnMediaServerResourcePtr &server);
    QnMediaServerResourcePtr server() const;

private:
    Q_SIGNAL void updateWindowTitleQueued();
    void updateWindowTitleLater();
    void updateWindowTitle();
    void updateCriterion();

    void at_camerasView_customContextMenuRequested(const QPoint &pos);
    void at_exportAction_triggered();
    void at_clipboardAction_triggered();
    void at_camerasView_doubleClicked(const QModelIndex &index);

private:
    Q_DISABLE_COPY(QnCameraListDialog)

    QScopedPointer<Ui::CameraListDialog> ui;
    QnCameraListModel *m_model;
    QnResourceSearchProxyModel *m_resourceSearch;
    QAction *m_selectAllAction;
    QAction *m_exportAction;
    QAction *m_clipboardAction;
    bool m_pendingWindowTitleUpdate;
};

#endif // QN_CAMERA_LIST_DIALOG_H

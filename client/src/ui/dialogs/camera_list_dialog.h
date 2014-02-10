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

class QnCameraListDialog: public QDialog, protected QnWorkbenchContextAware {
    Q_OBJECT

public:
    explicit QnCameraListDialog(QWidget *parent = NULL);
    virtual ~QnCameraListDialog();

    void setServer(const QnMediaServerResourcePtr &server);
    const QnMediaServerResourcePtr &server() const;

private slots:
    void at_searchStringChanged(const QString &text);
    void at_customContextMenuRequested(const QPoint &pos);
    void at_selectAllAction();
    void at_exportAction();
    void at_copyToClipboard();
    void at_gridDoubleClicked(const QModelIndex &index);
    void at_modelChanged();
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
    void at_resPool_resourceAdded(const QnResourcePtr &resource);

private:
    Q_DISABLE_COPY(QnCameraListDialog)

    QScopedPointer<Ui::CameraListDialog> ui;
    QnCameraListModel *m_model;
    QnResourceSearchProxyModel* m_resourceSearch;
    QAction* m_selectAllAction;
    QAction* m_exportAction;
    QAction* m_clipboardAction;
    QnMediaServerResourcePtr m_server;
};

#endif // QN_CAMERA_LIST_DIALOG_H

#ifndef __CAMERA_LIST_DIALOG_H__
#define __CAMERA_LIST_DIALOG_H__

#include <QDialog>
#include <QStandardItem>
#include <QAbstractItemModel>
#include <QModelIndex>

#include <ui/workbench/workbench_context_aware.h>

class QnCameraListModel;
class QnWorkbenchContext;
class QnResourceSearchProxyModel;

namespace Ui {
    class CameraListDialog;
}

class QnCameraListDialog: public QDialog, public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    explicit QnCameraListDialog(QWidget *parent = NULL, QnWorkbenchContext *context = NULL);
    virtual ~QnCameraListDialog();
private slots:
    void at_searchStringChanged(const QString& text);
    void at_customContextMenuRequested(const QPoint&);
    void at_copyToClipboard();
    void at_gridDoublelClicked(const QModelIndex& idx);
    void at_modelChanged();
private:
    Q_DISABLE_COPY(QnCameraListDialog)

    QScopedPointer<Ui::CameraListDialog> ui;
    QnCameraListModel *m_model;
    QnResourceSearchProxyModel* m_resourceSearch;
    QAction* m_clipboardAction;
};

#endif // __CAMERA_LIST_DIALOG_H__

#ifndef __CAMERA_LIST_DIALOG_H__
#define __CAMERA_LIST_DIALOG_H__

#include <QDialog>
#include <QStandardItem>
#include <QAbstractItemModel>
#include <QModelIndex>

#include "business/actions/abstract_business_action.h"
#include "business/events/abstract_business_event.h"
#include "core/resource/network_resource.h"

class QnCameraListModel;
class QnWorkbenchContext;
class QnResourceSearchProxyModel;

namespace Ui {
    class CameraListDialog;
}

class QnCameraListDialog: public QDialog
{
    Q_OBJECT

public:
    explicit QnCameraListDialog(QWidget *parent, QnWorkbenchContext *context);
    virtual ~QnCameraListDialog();
private slots:
    void at_searchStringChanged(const QString& text);
    void at_customContextMenuRequested(const QPoint&);
    void at_copyToClipboard();
    void at_gridDoublelClicked(const QModelIndex& idx);
private:
    Q_DISABLE_COPY(QnCameraListDialog)
 
    QScopedPointer<Ui::CameraListDialog> ui;
    QnCameraListModel *m_model;
    QnResourceSearchProxyModel* m_resourceSearch;
    QAction* m_clipboardAction;
    QnWorkbenchContext* m_context;
};

#endif // __CAMERA_LIST_DIALOG_H__

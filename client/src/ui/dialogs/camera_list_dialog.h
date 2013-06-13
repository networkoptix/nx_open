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
private:
    Q_DISABLE_COPY(QnCameraListDialog)
 
    QScopedPointer<Ui::CameraListDialog> ui;
    QnCameraListModel *m_model;
};

#endif // __CAMERA_LIST_DIALOG_H__

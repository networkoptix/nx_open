#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>
#include <api/video_server_cameras_data.h>

#include "button_box_dialog.h"

namespace Ui {
    class CameraAdditionDialog;
}

class QnCameraAdditionDialog: public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();
protected:
    virtual bool eventFilter(QObject *, QEvent *) override;
private:
    void fillTable(const QnCamerasFoundInfoList &cameras);

private slots: 
    void at_singleRadioButton_toggled(bool toggled);

    void at_scanButton_clicked();
    void at_addButton_clicked();

private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnVideoServerResourcePtr m_server;
};

#endif // CAMERA_ADDITION_DIALOG_H

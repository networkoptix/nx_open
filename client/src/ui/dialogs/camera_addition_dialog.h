#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>
#include <api/media_server_cameras_data.h>

namespace Ui {
    class CameraAdditionDialog;
}

namespace detail{
    class ManualCameraReplyProcessor: public QObject
    {
        Q_OBJECT
    public:

        ManualCameraReplyProcessor(QObject *parent = NULL):
            QObject(parent),
            m_cancelled(false)
        {}

        QnCamerasFoundInfoList camerasFound() const {
            return m_cameras;
        }

        bool addSuccess(){
            return (m_addStatus == 0);
        }

        bool isCancelled(){
            return m_cancelled;
        }

    signals:
        void replyReceived();

    public slots:
        void processSearchReply(const QnCamerasFoundInfoList &cameras)
        {
            if (m_cancelled)
                return;

            m_cameras = cameras;
            emit replyReceived();
        }

        void processAddReply(int status){
            if (m_cancelled)
                return;

            m_addStatus = status;
            emit replyReceived();
        }

        void cancel(){
            m_cancelled = true;
        }

    private:
        QnCamerasFoundInfoList m_cameras;
        int m_addStatus;
        bool m_cancelled;
    };
}

class QnCameraAdditionDialog: public QDialog {
    Q_OBJECT
public:
    explicit QnCameraAdditionDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();
private:
    void fillTable(const QnCamerasFoundInfoList &cameras);
    void removeAddedCameras();
    void updateSubnetMode();

private slots: 
    void at_startIPLineEdit_textChanged(QString value);
    void at_startIPLineEdit_editingFinished();
    void at_endIPLineEdit_textChanged(QString value);
    void at_camerasTable_cellChanged(int row, int column);
    void at_camerasTable_cellClicked(int row, int column);

    void at_scanButton_clicked();
    void at_addButton_clicked();
    void at_subnetCheckbox_toggled(bool toggled);

private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnMediaServerResourcePtr m_server;

    bool m_inIpRangeEdit;
    bool m_subnetMode;
};

#endif // CAMERA_ADDITION_DIALOG_H

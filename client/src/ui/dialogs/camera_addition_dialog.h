#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

namespace Ui {
    class CameraAdditionDialog;
}

namespace detail {
    struct CamerasFoundInfo
    {
        CamerasFoundInfo(): freeSpace(0), usedSpace(0), errorCode(0) {}
        CamerasFoundInfo(qint64 _freeSpace, qint64 _usedSpace, int _errorCode): freeSpace(_freeSpace), usedSpace(_usedSpace), errorCode(_errorCode) {}
        qint64 freeSpace;
        qint64 usedSpace;
        int errorCode;
    };
    typedef QMap<int, CamerasFoundInfo> CamerasFoundInfoMap;

    class CheckCamerasFoundReplyProcessor: public QObject
    {
        Q_OBJECT
    public:

        CheckCamerasFoundReplyProcessor(QObject *parent = NULL):
            QObject(parent),
            m_cancelled(false)
        {}

        CamerasFoundInfoMap camerasFound() const {
            return m_cameras;
        }

        QByteArray getData() const {
            return m_data;
        }

    signals:
        void replyReceived();

    public slots:
        void processReply(int status, const QByteArray &data) //TODO: #gdm change profile
        {
            if (m_cancelled)
                return;

            m_data.append(data);
            qDebug() << "reply status" << status;
            qDebug() << "data received" << data;
            emit replyReceived();
        }

        void cancel(){
            m_cancelled = true;
            qDebug() << "request cancelled";
        }

    private:
        CamerasFoundInfoMap m_cameras;
        QByteArray m_data;
        bool m_cancelled;
    };

} // namespace detail

struct QnCamerasAddInfo{
    bool checked;
    QString address;
    QString name;
};
typedef QList<QnCamerasAddInfo> QnCamerasAddInfoList;


class QnCameraAdditionDialog: public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();

    QnCamerasAddInfoList cameras() const;
private:
    int addTableRow(const QByteArray &data);

private slots: 
    void at_scanButton_clicked();
    void at_singleRadioButton_toggled(bool toggled);

private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnVideoServerResourcePtr m_server;
};

#endif // CAMERA_ADDITION_DIALOG_H

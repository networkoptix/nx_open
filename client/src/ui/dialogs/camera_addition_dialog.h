#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtGui/QDialog>
#include <QtGui/QHeaderView>

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

        bool isSuccess(){
            return (m_status == 0);
        }

        bool isCancelled(){
            return m_cancelled;
        }

        QString getLastError() const {
            return m_lastError;
        }

    signals:
        void replyReceived();

    public slots:
        void processSearchReply(const QnCamerasFoundInfoList &cameras) {
            if (m_cancelled)
                return;

            m_status = 0;
            m_lastError = QString();
            m_cameras = cameras;

            /*
            m_cameras
                    << QnCamerasFoundInfo(tr("http://10.0.0.2/testcamera/tescamera_url/tescamera_url/tescamera_url/tescamera_url"),
                                            tr("Test Camera 0"),
                                            tr("Network Optix"))
                    << QnCamerasFoundInfo(tr("http://10.0.0.2/testcamera/tescamera_url/tescamera_url/tescamera_url/tescamera_url"),
                                            tr("Test Camera 1 Omg ololo camera"),
                                            tr("Network Optix"))
                    << QnCamerasFoundInfo(tr("http://10.0.0.2/testcamera/tescamera_url/tescamera_url/tescamera_url/tescamera_url"),
                                            tr("Test Camera 2"),
                                            tr("Network Optix the best menufacturer"));

            */
            emit replyReceived();
        }

        void processSearchError(int status, const QString &error) {
            if (m_cancelled)
                return;

            m_status = status;
            m_lastError = error;
            emit replyReceived();
        }

        void processAddReply(int status) {
            if (m_cancelled)
                return;

            m_status = status;
            emit replyReceived();
        }

        void cancel() {
            m_cancelled = true;
        }

    private:
        QnCamerasFoundInfoList m_cameras;
        int m_status;
        bool m_cancelled;
        QString m_lastError;
    };
}

class QnCheckBoxedHeaderView: public QHeaderView {
    Q_OBJECT
    typedef QHeaderView base_type;
public:
    explicit QnCheckBoxedHeaderView(QWidget *parent = 0);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);
signals:
    void checkStateChanged(Qt::CheckState state);
protected:
    virtual void paintEvent(QPaintEvent *e) override;
    virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
    virtual QSize sectionSizeFromContents(int logicalIndex) const override;
private slots:
    void at_sectionClicked(int logicalIndex);
private:
    Qt::CheckState m_checkState;
};

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
    void at_header_checkStateChanged(Qt::CheckState state);

    void at_scanButton_clicked();
    void at_addButton_clicked();
    void at_subnetCheckbox_toggled(bool toggled);

private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnMediaServerResourcePtr m_server;
    QnCheckBoxedHeaderView* m_header;

    bool m_inIpRangeEdit;
    bool m_subnetMode;
    bool m_inCheckStateChange;
};

#endif // CAMERA_ADDITION_DIALOG_H

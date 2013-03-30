#ifndef CAMERA_ADDITION_DIALOG_H
#define CAMERA_ADDITION_DIALOG_H

#include <QtCore/QEventLoop>

#include <QtGui/QDialog>
#include <QtGui/QHeaderView>

#include <core/resource/resource_fwd.h>
#include <api/media_server_cameras_data.h>

namespace Ui {
    class CameraAdditionDialog;
}

namespace detail{
    enum ProcessorState {
        Init,
        Progress,
        Cancelled,
        Error,
        Success
    };

    class ManualCameraReplyProcessor: public QObject
    {
        Q_OBJECT
    public:
        ManualCameraReplyProcessor(QObject *parent = NULL):
            QObject(parent),
            m_state(Init),
            m_eventLoop(new QEventLoop(this))
        {
            connect(this, SIGNAL(replyReceived()),  m_eventLoop, SLOT(quit()));
        }

        QnCamerasFoundInfoList camerasFound() const {
            if (m_state != Success)
                return QnCamerasFoundInfoList();
            return m_cameras;
        }

        ProcessorState state() const {
            return m_state;
        }

        QString getLastError() const {
            return m_lastError;
        }

    signals:
        void replyReceived();

    public slots:
        void processSearchReply(const QnCamerasFoundInfoList &cameras) {
            if (m_state != Progress)
                return;

            m_state = Success;
            m_lastError = QString();
            m_cameras = cameras;
            emit replyReceived();
        }

        void processSearchError(int status, const QString &error) {
            Q_UNUSED(status)
            if (m_state != Progress)
                return;

            m_state = Error;
            m_lastError = error;
            emit replyReceived();
        }

        void processAddReply(int status) {
            if (m_state != Progress)
                return;

            m_state = status == 0 ? Success : Error;
            m_lastError = QString();
            emit replyReceived();
        }

        void start() {
            Q_ASSERT(m_state == Init);
            m_state = Progress;
            m_eventLoop->exec();
        }

        void cancel() {
            if (m_state != Progress)
                qWarning() << "reply processor state error: requsted Progress while" << m_state;
            m_state = Cancelled;
            m_eventLoop->quit();
        }

    private:
        QnCamerasFoundInfoList m_cameras;
        QString m_lastError;
        ProcessorState m_state;
        QEventLoop* m_eventLoop;
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
    explicit QnCameraAdditionDialog(QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();

    void setServer(const QnMediaServerResourcePtr &server);
signals:
    void serverChanged();

private:
    void clearTable();
    void fillTable(const QnCamerasFoundInfoList &cameras);
    void removeAddedCameras();
    void updateSubnetMode();
    bool ensureServerOnline();
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
    void at_resPool_resourceChanged(const QnResourcePtr &resource);
    void at_resPool_resourceRemoved(const QnResourcePtr &resource);
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

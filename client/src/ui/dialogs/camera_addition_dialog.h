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

        CheckCamerasFoundReplyProcessor(QObject *parent = NULL): QObject(parent) {}

        CamerasFoundInfoMap freeSpaceInfo() const {
            return m_freeSpace;
        }

    signals:
        void replyReceived(int status, qint64 freeSpace, qint64 usedSpace, int handle);

    public slots:
        void processReply(int status, qint64 freeSpace, qint64 usedSpace,  int handle) 
        {
            int errCode = status == 0 ? (freeSpace > 0 ? 0 : -1) : -2;
            m_freeSpace.insert(handle, CamerasFoundInfo(freeSpace, usedSpace, errCode));
            emit replyReceived(status, freeSpace, usedSpace, handle);
        }

    private:
        CamerasFoundInfoMap m_freeSpace;
    };

} // namespace detail


class QnCameraAdditionDialog: public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnCameraAdditionDialog(const QnVideoServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnCameraAdditionDialog();
private:
    int addTableRow(int id, const QString &url, int spaceLimitGb);

    void setTableStorages(const QnAbstractStorageResourceList &storages);
    QnAbstractStorageResourceList tableStorages() const;

    bool validateStorages(const QnAbstractStorageResourceList &storages, QString *errorString);

    void updateSpaceLimitCell(int row, bool force = false);

private slots: 
    void at_scanButton_clicked();

private:
    Q_DISABLE_COPY(QnCameraAdditionDialog)

    QScopedPointer<Ui::CameraAdditionDialog> ui;
    QnVideoServerResourcePtr m_server;
};

#endif // CAMERA_ADDITION_DIALOG_H

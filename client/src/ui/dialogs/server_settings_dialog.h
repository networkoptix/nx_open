#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>
#include <api/model/storage_space_data.h>

#include "button_box_dialog.h"


namespace Ui {
    class ServerSettingsDialog;
}

namespace detail {
    static const int INVALID_PATH = -1;
    static const int SERVER_ERROR = -2;
    struct FreeSpaceInfo 
    {
        FreeSpaceInfo(): freeSpace(0), totalSpace(0), errorCode(0) {}
        FreeSpaceInfo(qint64 _freeSpace, qint64 _totalSpace, int _errorCode): freeSpace(_freeSpace), totalSpace(_totalSpace), errorCode(_errorCode) {}
        qint64 freeSpace;
        qint64 totalSpace;
        int errorCode;
    };
    typedef QMap<int, FreeSpaceInfo> FreeSpaceMap;

    class CheckFreeSpaceReplyProcessor: public QObject 
    {
        Q_OBJECT
    public:

        CheckFreeSpaceReplyProcessor(QObject *parent = NULL): QObject(parent) {}

        FreeSpaceMap freeSpaceInfo() const {
            return m_freeSpace;
        }

    signals:
        void replyReceived(int status, qint64 freeSpace, qint64 usedSpace, int handle);

    public slots:
        void processReply(int status, qint64 freeSpace, qint64 totalSpace,  int handle) 
        {
            int errCode = status == 0 ? (freeSpace > 0 ? 0 : INVALID_PATH) : SERVER_ERROR;
            m_freeSpace.insert(handle, FreeSpaceInfo(freeSpace, totalSpace, errCode));
            emit replyReceived(status, freeSpace, totalSpace, handle);
        }

    private:
        FreeSpaceMap m_freeSpace;
    };

} // namespace detail


class QnServerSettingsDialog: public QnButtonBoxDialog {
    Q_OBJECT;

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnServerSettingsDialog(const QnMediaServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

private:
    struct StorageItem;

    void updateFromResources();
    void submitToResources();

    void addTableItem(const StorageItem &item);
    void setTableItems(const QList<StorageItem> &items);
    QList<StorageItem> &items() const;

    int addTableRow(int id, const QString &url, int spaceLimitGb);

    //void setTableStorages(const QnAbstractStorageResourceList &storages);
    //QnAbstractStorageResourceList tableStorages() const;

    //bool validateStorages(const QnAbstractStorageResourceList &storages);

    //void updateSpaceLimitCell(int row, bool force = false);

private slots: 
    void at_storageAddButton_clicked();
    void at_storageRemoveButton_clicked();
    void at_storagesTable_cellChanged(int row, int column);

    void at_replyReceived(int status, const QnStorageSpaceDataList &dataList, int handle);

private:
    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnMediaServerResourcePtr m_server;

    bool m_hasStorageChanges;
};

#endif // SERVER_SETTINGS_DIALOG_H

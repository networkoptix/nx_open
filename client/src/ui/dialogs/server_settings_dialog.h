#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

namespace Ui {
    class ServerSettingsDialog;
}

namespace detail {
	struct FreeSpaceInfo 
	{
		FreeSpaceInfo(): freeSpace(0), usedSpace(0) {}
		FreeSpaceInfo(qint64 _freeSpace, qint64 _usedSpace): freeSpace(_freeSpace), usedSpace(_usedSpace) {}
		qint64 freeSpace;
		qint64 usedSpace;
	};
	typedef QMap<int, FreeSpaceInfo> FreeSpaceMap;

    class CheckFreeSpaceReplyProcessor: public QObject 
	{
        Q_OBJECT;
    public:

        CheckFreeSpaceReplyProcessor(QObject *parent = NULL): QObject(parent) {}

		FreeSpaceMap freeSpaceInfo() const {
			return m_freeSpace;
		}

    signals:
        void replyReceived(int status, qint64 freeSpace, qint64 usedSpace, int handle);

    public slots:
        void processReply(int status, qint64 freeSpace, qint64 usedSpace,  int handle) {
			m_freeSpace.insert(handle, FreeSpaceInfo(freeSpace, usedSpace));
            emit replyReceived(status, freeSpace, usedSpace, handle);
        }

    private:
		FreeSpaceMap m_freeSpace;
    };

} // namespace detail


class QnServerSettingsDialog: public QnButtonBoxDialog {
    Q_OBJECT;

    typedef QnButtonBoxDialog base_type;

public:
    explicit QnServerSettingsDialog(const QnVideoServerResourcePtr &server, QWidget *parent = NULL);
    virtual ~QnServerSettingsDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

private:
    void updateFromResources();
    void submitToResources();

    int addTableRow(int id, const QString &url, int spaceLimitGb);

    void setTableStorages(const QnAbstractStorageResourceList &storages);
    QnAbstractStorageResourceList tableStorages() const;

    bool validateStorages(const QnAbstractStorageResourceList &storages, QString *errorString);

    void updateSpaceLimitCell(int row, bool force = false);

private slots: 
    void at_addStorageButton_clicked();
    void at_removeStorageButton_clicked();
    void at_storagesTable_cellChanged(int row, int column);

private:
    Q_DISABLE_COPY(QnServerSettingsDialog);

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnVideoServerResourcePtr m_server;

    bool m_hasStorageChanges;
};

#endif // SERVER_SETTINGS_DIALOG_H

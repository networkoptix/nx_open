#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include <core/resource/resource_fwd.h>

#include "button_box_dialog.h"

namespace Ui {
    class ServerSettingsDialog;
}

namespace detail {
    class CheckPathReplyProcessor: public QObject {
        Q_OBJECT;
    public:
        CheckPathReplyProcessor(QObject *parent = NULL): QObject(parent) {}

        const QList<int> &invalidHandles() const {
            return m_invalidHandles;
        }

    signals:
        void replyReceived(int status, bool result, int handle);

    public slots:
        void processReply(int status, bool result, int handle) {
            if(status != 0 || !result)
                m_invalidHandles.push_back(handle);

            emit replyReceived(status, result, handle);
        }

    private:
        QList<int> m_invalidHandles;
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

    int addTableRow(const QString &url, int spaceLimitGb);

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
};

#endif // SERVER_SETTINGS_DIALOG_H

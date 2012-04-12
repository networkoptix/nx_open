#ifndef SERVER_SETTINGS_DIALOG_H
#define SERVER_SETTINGS_DIALOG_H

#include <QtGui/QDialog>

#include "api/AppServerConnection.h"
#include "core/resource/video_server.h"

namespace Ui {
    class ServerSettingsDialog;
}

class ServerSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ServerSettingsDialog(QnVideoServerResourcePtr server, QWidget *parent = NULL);
    virtual ~ServerSettingsDialog();

public slots:
    virtual void accept() override;
    virtual void reject() override;

protected slots: 
    void addClicked();
    void removeClicked();
    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    QSpinBox* createSpinCellWidget(QWidget* parent);
    void initView();
    bool saveToModel();
    bool validateStorages(const QnAbstractStorageResourceList& storages, QString& errorString);
    void save();

private:
    Q_DISABLE_COPY(ServerSettingsDialog)

    QScopedPointer<Ui::ServerSettingsDialog> ui;
    QnVideoServerResourcePtr m_server;
    QnAppServerConnectionPtr m_connection;
};

#endif // SERVER_SETTINGS_DIALOG_H

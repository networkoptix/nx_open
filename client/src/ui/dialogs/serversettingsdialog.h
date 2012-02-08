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
    explicit ServerSettingsDialog(QWidget *parent, QnVideoServerResourcePtr server);
    ~ServerSettingsDialog();

public slots:
    virtual void accept();
    virtual void reject();
    virtual void buttonClicked(QAbstractButton *button);

    void addClicked();
    void removeClicked();

    void requestFinished(int status, const QByteArray& errorString, QnResourceList resources, int handle);

private:
    QSpinBox* createSpinCellWidget(QWidget* parent);
    void initView();
    bool saveToModel();
    bool validateStorages(const QnStorageResourceList& storages, QString& errorString);

    void save();

private:
    Q_DISABLE_COPY(ServerSettingsDialog)

    QnVideoServerResourcePtr m_server;

    QScopedPointer<Ui::ServerSettingsDialog> ui;

    QnAppServerConnectionPtr m_connection;
};

#endif // SERVER_SETTINGS_DIALOG_H

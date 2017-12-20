#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <api/model/manual_camera_seach_reply.h>
#include <utils/common/connective.h>

namespace nx {
namespace client {
namespace desktop {

class ManualDeviceSearcher: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    enum class State
    {
        InitializationError,
        Searching,
        Finished
    };

    ManualDeviceSearcher(
        const QnMediaServerResourcePtr& server,
        const QString& address,
        const QString& login,
        const QString& password,
        int port);

    ManualDeviceSearcher(
        const QnMediaServerResourcePtr& server,
        const QString& startAddress,
        const QString& endAddress,
        const QString& login,
        const QString& password,
        int port);

    virtual ~ManualDeviceSearcher();

    QnManualResourceSearchStatus::State progress() const;

    QString lastErrorText() const;

    QnMediaServerResourcePtr server() const;

    QnManualResourceSearchList devices() const;

    bool searching() const;

    void stop();

    const QString& login() const;

    const QString& password() const;

signals:
    void progressChanged();
    void lastErrorTextChanged();

private:
    void init();

    bool checkServer();
    bool checkUrl(const QString& stringUrl);
    bool checkAddresses(
        const QString& startAddress,
        const QString& endAddress);
    void searchForDevices(
        const QString& startAddress,
        const QString& endAddress,
        const QString& login,
        const QString& password,
        int port);

    void setProgress(QnManualResourceSearchStatus::State value);
    void setLastErrorText(const QString& text);

    void updateDevices(const QnManualResourceSearchList& devices);

    void updateProgress();
    void handleProgressChanged();

signals:
    void devicesAdded(const QnManualResourceSearchList& devices);
    void devicesRemoved(const QStringList& deviceIds);

private:
    Q_SLOT void handleStartSearchReply(int status, const QVariant& reply, int handle);
    Q_SLOT void handleStopSearchReply(int status, const QVariant& reply, int handle);
    Q_SLOT void handleUpdateStatusReply(int status, const QVariant& reply, int handle);

private:
    const QnMediaServerResourcePtr m_server;
    const QString m_login;
    const QString m_password;
    QnManualResourceSearchStatus::State m_progress = QnManualResourceSearchStatus::Init;
    QString m_lastErrorText;

    QTimer m_updateProgressTimer;
    QnUuid m_searchProcessId;

    using DevicesHash = QHash<QString, QnManualResourceSearchEntry>;
    DevicesHash m_devices;
};

} // namespace desktop
} // namespace client
} // namespace nx

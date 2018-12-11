#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <core/resource/resource_fwd.h>
#include <api/model/manual_camera_seach_reply.h>
#include <utils/common/connective.h>

namespace nx::vms::client::desktop {

class ManualDeviceSearcher: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    ManualDeviceSearcher(
        const QnMediaServerResourcePtr& server,
        const QString& url,
        const QString& login,
        const QString& password,
        std::optional<int> port = {});

    ManualDeviceSearcher(
        const QnMediaServerResourcePtr& server,
        const QString& startAddress,
        const QString& endAddress,
        const QString& login,
        const QString& password,
        std::optional<int> port = {});

    virtual ~ManualDeviceSearcher() override;

    const QnManualResourceSearchStatus& status() const;

    QString initialError() const;

    QnMediaServerResourcePtr server() const;

    QnManualResourceSearchList devices() const;

    bool searching() const;

    void stop();

    const QString& login() const;

    const QString& password() const;

signals:
    void statusChanged();
    void lastErrorTextChanged();
    void devicesAdded(const QnManualResourceSearchList& devices);
    void devicesRemoved(const QStringList& deviceIds);

private:
    void init();

    bool checkServer();
    bool checkUrl(const QString& stringUrl);
    bool checkAddresses(
        const QString& startAddress,
        const QString& endAddress);
    void searchForDevices(
        const QString& urlOrStartAddress,
        const QString& endAddress,
        const QString& login,
        const QString& password,
        std::optional<int> port);

    void abort();
    void setStatus(const QnManualResourceSearchStatus& value);
    void setLastErrorText(const QString& text);

    void updateDevices(const QnManualResourceSearchList& devices);

    void updateStatus();
    void handleProgressChanged();

private:
    const QnMediaServerResourcePtr m_server;
    const QString m_login;
    const QString m_password;
    QnManualResourceSearchStatus m_status = {QnManualResourceSearchStatus::Finished, 0, 0};
    QString m_lastErrorText;

    QTimer m_updateProgressTimer;
    QnUuid m_searchProcessId;

    using DevicesHash = QHash<QString, QnManualResourceSearchEntry>;
    DevicesHash m_devices;
};

} // namespace nx::vms::client::desktop

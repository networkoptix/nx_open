#include "manual_device_searcher.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>

namespace {

template<typename SetType>
SetType subtract(SetType first, const SetType& second)
{
    return first.subtract(second);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

ManualDeviceSearcher::ManualDeviceSearcher(
    const QnMediaServerResourcePtr& server,
    const QString& address,
    const QString& login,
    const QString& password,
    int port)
    :
    m_server(server),
    m_login(login),
    m_password(password)
{
    if (!checkServer() || !checkUrl(address))
        return;

    init();
    searchForDevices(address, QString(), login, password, port);
}

ManualDeviceSearcher::ManualDeviceSearcher(
    const QnMediaServerResourcePtr& server,
    const QString& startAddress,
    const QString& endAddress,
    const QString& login,
    const QString& password,
    int port)
    :
    m_server(server),
    m_login(login),
    m_password(password)
{
    if (!checkServer() || !checkAddresses(startAddress, endAddress))
        return;

    init();
    searchForDevices(startAddress, endAddress, login, password, port);
}

ManualDeviceSearcher::~ManualDeviceSearcher()
{
    stop(); //< Last try to stop search.
}

QnManualResourceSearchStatus::State ManualDeviceSearcher::progress() const
{
    return m_progress;
}

QString ManualDeviceSearcher::initialError() const
{
    return m_lastErrorText;
}

QnMediaServerResourcePtr ManualDeviceSearcher::server() const
{
    return m_server;
}

QnManualResourceSearchList ManualDeviceSearcher::devices() const
{
    return m_devices.values();
}

void ManualDeviceSearcher::stop()
{
    if (!m_server || !searching())
        return;

    const auto stopCallback =
        [this, guard = QPointer<ManualDeviceSearcher>(this)](
            bool success,
            rest::Handle /*handle*/,
            const QnJsonRestResult& result)
        {
            if (!guard)
                return;

            if (!success || result.error != QnRestResult::NoError)
                stop(); //< Try to stop one more time.
            else if (m_progress != QnManualResourceSearchStatus::Finished)
                setProgress(QnManualResourceSearchStatus::Aborted);
        };

    m_server->restConnection()->searchCameraStop(
        m_searchProcessId, stopCallback, QThread::currentThread());
}

const QString& ManualDeviceSearcher::login() const
{
    return m_login;
}

const QString& ManualDeviceSearcher::password() const
{
    return m_password;
}

void ManualDeviceSearcher::init()
{
    static constexpr auto kUpdateProgressIntervalMs = 1000;
    m_updateProgressTimer.setInterval(kUpdateProgressIntervalMs);
    connect(&m_updateProgressTimer, &QTimer::timeout,
        this, &ManualDeviceSearcher::updateProgress);

    connect(m_server, &QnMediaServerResource::statusChanged, this,
        [this]()
        {
            if (m_server->getStatus() != Qn::Online)
                setProgress(QnManualResourceSearchStatus::Aborted);
        });
}

bool ManualDeviceSearcher::checkServer()
{
    if (!m_server)
        setLastErrorText(tr("Server is not specified"));
    else if (m_server->getStatus() != Qn::Online)
        setLastErrorText(tr("Server offline"));
    else
         return true;

    setProgress(QnManualResourceSearchStatus::Aborted);
    return false;
}

bool ManualDeviceSearcher::checkUrl(const QString& stringUrl)
{
    if (QUrl::fromUserInput(stringUrl).isValid())
        return true;

    setLastErrorText(
        tr("Device address field must contain a valid URL, IP address, or RTSP link."));

    setProgress(QnManualResourceSearchStatus::Aborted);
    return false;
}

bool ManualDeviceSearcher::checkAddresses(
    const QString& startAddress,
    const QString& endAddress)
{
    const auto startHost = QHostAddress(startAddress);
    const auto endHost = QHostAddress(endAddress);
    if (startHost.toIPv4Address() > endHost.toIPv4Address())
        setLastErrorText(tr("First address in range is greater than the last one."));
    else if (!endHost.isInSubnet(startHost, 8))
        setLastErrorText(tr("The specified IP address range has more than 255 addresses."));
    else
        return true;

    setProgress(QnManualResourceSearchStatus::Aborted);
    return false;
}

void ManualDeviceSearcher::searchForDevices(
    const QString& startAddress,
    const QString& endAddress,
    const QString& login,
    const QString& password,
    int port)
{
    const auto startCallback =
        [this, guard = QPointer<ManualDeviceSearcher>(this)](
            bool success,
            rest::Handle /*handle*/,
            const QnJsonRestResult& result)
        {
            if (!guard)
                return;

            if (!success || result.error != QnRestResult::NoError)
            {
                setLastErrorText(tr("Can't start searching process"));
                setProgress(QnManualResourceSearchStatus::Aborted);
            }
            else
            {
                const auto reply = result.deserialized<QnManualCameraSearchReply>();
                m_searchProcessId = reply.processUuid;
                m_updateProgressTimer.start();
            }
        };


    m_server->restConnection()->searchCameraStart(
        startAddress, endAddress, login, password, port, startCallback, QThread::currentThread());
}

bool ManualDeviceSearcher::searching() const
{
    return m_progress != QnManualResourceSearchStatus::Aborted
        && m_progress != QnManualResourceSearchStatus::Finished;
}

void ManualDeviceSearcher::setProgress(QnManualResourceSearchStatus::State value)
{
    if (m_progress == value)
        return;

    m_progress = value;
    emit progressChanged();
}

void ManualDeviceSearcher::setLastErrorText(const QString& text)
{
    if (m_lastErrorText == text)
        return;

    m_lastErrorText = text;
    emit lastErrorTextChanged();
}

void ManualDeviceSearcher::updateDevices(const QnManualResourceSearchList& devices)
{
    DevicesHash newDevices;
    for (const auto& device: devices)
        newDevices.insert(device.uniqueId, device);

    const auto currentIds = m_devices.keys().toSet();
    const auto newIds = newDevices.keys().toSet();
    const auto addedIds = subtract(newIds, currentIds);
    const auto removedIds = subtract(currentIds, newIds);

    for (const auto& idForRemove: removedIds)
        m_devices.remove(idForRemove);

    emit devicesRemoved(removedIds.toList());

    QnManualResourceSearchList addedDevices;
    for (const auto& idForAdd: addedIds)
    {
        const auto& device = newDevices[idForAdd];
        addedDevices.append(device);
        m_devices.insert(idForAdd, device);
    }

    emit devicesAdded(addedDevices);
}

void ManualDeviceSearcher::updateProgress()
{
    if (!searching())
        return;

    const auto updateProgressCallback =
        [this, guard = QPointer<ManualDeviceSearcher>(this)](
            bool success,
            rest::Handle /*handle*/,
            const QnJsonRestResult& result)
        {
            if (!guard)
                return;

            if (!success || result.error != QnRestResult::NoError)
                return;

            NX_EXPECT(!m_searchProcessId.isNull());

            const auto reply = result.deserialized<QnManualCameraSearchReply>();
            setProgress(static_cast<QnManualResourceSearchStatus::State>(reply.status.state));
            updateDevices(reply.cameras);
        };

    m_server->restConnection()->searchCameraStatus(
        m_searchProcessId, updateProgressCallback, QThread::currentThread());
}

} // namespace desktop
} // namespace client
} // namespace nx

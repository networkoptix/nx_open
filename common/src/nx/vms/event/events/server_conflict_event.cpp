#include "server_conflict_event.h"

#include <core/resource/resource.h>
#include <network/module_information.h>

namespace {
static const QChar kDelimiter(L'\n');
} // namespace

QString QnCameraConflictList::encode() const
{
    QString result;
    for (auto iter = camerasByServer.cbegin(); iter != camerasByServer.cend(); ++iter)
    {
        const auto& server = iter.key();
        const auto& cameras = iter.value();

        result.append(server);
        result.append(kDelimiter);

        result.append(QString::number(cameras.size()));
        result.append(kDelimiter);

        for (const auto& camera: cameras)
        {
            result.append(camera);
            result.append(kDelimiter);
        }
    }
    return result.left(result.size()-1);
}

void QnCameraConflictList::decode(const QString& encoded)
{
    enum class EncodeState
    {
        server,
        size,
        cameras
    };

    auto state = EncodeState::server;
    int counter = 0;

    QString curServer;
    for (const auto& value: encoded.split(kDelimiter))
    {
        switch (state)
        {
            case EncodeState::server:
                curServer = value;
                state = EncodeState::size;
                break;

            case EncodeState::size:
                counter = value.toInt();
                state = EncodeState::cameras;
                break;

            case EncodeState::cameras:
                --counter;
                camerasByServer[curServer].append(value);
                if (counter <= 0)
                    state = EncodeState::server;
                break;
        }
    }
}

namespace nx {
namespace vms {
namespace event {

ServerConflictEvent::ServerConflictEvent(
    const QnResourcePtr& server,
    qint64 timeStamp,
    const QnCameraConflictList& conflictList)
    :
    base_type(serverConflictEvent, server, timeStamp),
    m_cameraConflicts(conflictList)
{
    m_caption = m_cameraConflicts.sourceServer;
    m_description = m_cameraConflicts.encode();
}

ServerConflictEvent::ServerConflictEvent(
    const QnResourcePtr& server,
    qint64 timeStamp,
    const QnModuleInformation& conflictModule,
    const QUrl& conflictUrl)
    :
    base_type(serverConflictEvent, server, timeStamp)
{
    Q_UNUSED(conflictModule)
    m_caption = lit("%1:%2").arg(conflictUrl.host()).arg(conflictUrl.port());
}

} // namespace event
} // namespace vms
} // namespace nx

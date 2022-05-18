// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_conflict_list.h"

namespace nx::vms::rules {

namespace {

static const QChar kDelimiter('\n');

} // namespace

QString CameraConflictList::encode() const
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

void CameraConflictList::decode(const QString& encoded)
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

} // namespace nx::vms::rules

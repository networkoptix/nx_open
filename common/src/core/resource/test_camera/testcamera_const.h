#pragma once

#include <QtCore/QByteArray>

class TestCamConst
{
public:
    // TODO: #Elric #enum
    enum MediaType { MediaType_H264, MediaType_MJPEG, MediaType_Unknown };
    static const int DISCOVERY_PORT;
    static const QByteArray TEST_CAMERA_FIND_MSG;
    static const QByteArray TEST_CAMERA_ID_MSG;
};

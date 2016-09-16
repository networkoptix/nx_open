#ifndef __TEST_CAMERA_CONST_H__
#define __TEST_CAMERA_CONST_H__

#ifdef ENABLE_TEST_CAMERA

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


#endif // #ifdef ENABLE_TEST_CAMERA
#endif // __TEST_CAMERA_CONST_H__

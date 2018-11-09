#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QVector>
#include <QtCore/QByteArray>

namespace nx::streaming {

struct Sdp {
    enum class MediaType {
        Video,
        Audio,
        Metadata,
        Unknown
    };

    struct Rtpmap
    {
        QString codecName;
        int clockRate = 0;
        int format = -1;
        int channels = 0;
    };

    struct Fmtp
    {
        int format = -1;
        QStringList params;
    };

    struct Media
    {
        int format = -1;
        MediaType mediaType = MediaType::Unknown;
        QString control;
        bool sendOnly = false;
        uint32_t ssrc = 0;
        Rtpmap rtpmap;
        Fmtp fmtp;
        QStringList sdpAttributes;
    };

    /**
    * Parse SDP session description according to RFC-4566: Session Description Protocol
    * https://tools.ietf.org/html/rfc4566
    * Current implementation is not complete and may fail on correct data
    * (though it work fine in all tests? we've done).
    */
    void parse(const QString& sdp);

    std::vector<Media> media;
};

} // namespace nx::streaming

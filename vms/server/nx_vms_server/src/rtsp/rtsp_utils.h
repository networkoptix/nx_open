#pragma once

#include <QtCore/QString>
#include <QtCore/QSize>
#include <QtCore/QUrlQuery>

#include <optional>
#include <nx/streaming/media_data_packet.h>

extern "C"
{
    #include <libavcodec/avcodec.h>
};

namespace nx::rtsp {

struct UrlParams
{
    bool parse(const QUrlQuery& query);
    QString getParseError() { return error; }

    std::optional<AVCodecID> codec;
    std::optional<int64_t> position;
    std::optional<QSize> resolution;
    std::optional<MediaQuality> quality;
    std::optional<bool> onvifReplay;
    //TODO std::optional<int> rotation;
    //TODO std::optional<QString> speed;
    //TODO bool multiplePayloadTypes = false;

private:
    QString error;
};

AVCodecID findEncoderCodecId(const QString& codecName);

QSize parseResolution(const QString& str);

} // namespace nx::rtsp

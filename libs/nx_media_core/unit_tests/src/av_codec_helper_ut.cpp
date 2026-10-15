// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/av_codec_helper.h>

namespace nx::media::test {

const std::map<QString, AVCodecID> kTestCases = {
    // Video codecs.
    {"H264", AV_CODEC_ID_H264},
    {"H265", AV_CODEC_ID_HEVC},
    {"MJPEG", AV_CODEC_ID_MJPEG},
    {"AV1", AV_CODEC_ID_AV1},
    {"MPEG4", AV_CODEC_ID_MPEG4},
    {"MPEG2VIDEO", AV_CODEC_ID_MPEG2VIDEO},
    {"VP8", AV_CODEC_ID_VP8},

    // Audio codecs.
    {"AAC", AV_CODEC_ID_AAC},
    {"FLAC", AV_CODEC_ID_FLAC},
    {"Dolby", AV_CODEC_ID_AC3},
    {"AMR-NB", AV_CODEC_ID_AMR_NB},
};

TEST(AvCodecHelper, codecIdToString)
{
    for (const auto& [name, codecId]: kTestCases)
        EXPECT_EQ(QnAvCodecHelper::codecIdToString(codecId), name) << name.toStdString();

    // Invalid is empty.
    EXPECT_EQ(QString(), QnAvCodecHelper::codecIdToString(AV_CODEC_ID_NONE));
}

TEST(AvCodecHelper, codecIdFromString)
{
    auto toAlternatingCase = [](QString name)
    {
        for (qsizetype i = 0; i < name.size(); ++i)
        {
            QChar& c = name[i];
            c = (i % 2 == 0) ? c.toUpper() : c.toLower();
        }
        return name;
    };

    for (const auto& [name, codecId]: kTestCases)
    {
        for (QString value: {name, name.toUpper(), name.toLower(), toAlternatingCase(name)})
        {
            EXPECT_EQ(QnAvCodecHelper::codecIdFromString(value), codecId)
                << name.toStdString() << " (" << value.toStdString() << ")";
        }
    }

    for (QString name: {QString(), QString(""), QString("asdqwe")})
    {
        EXPECT_EQ(QnAvCodecHelper::codecIdFromString(name), AV_CODEC_ID_NONE)
            << name.toStdString();
    }
}

} // namespace nx::media::test

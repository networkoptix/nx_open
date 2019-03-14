#include <gtest/gtest.h>

#include <nx/streaming/media_context.h>
#include <nx/streaming/av_codec_media_context.h>
#include <nx/streaming/basic_media_context.h>
#include <nx/streaming/media_context_serializable_data.h>
#include <nx/utils/log/assert.h>
#include <utils/media/ffmpeg_helper.h>
#include <utils/media/av_codec_helper.h>

/**
 * Fill with dummy data the fields of AVCodecContext which are present in QnMediaContext.
 */
void fillDummyAvCodecContext(AVCodecContext* context)
{
    static const char* const kRcEqDummy = "rc_eq dummy string";
    static const char* const kExtradataDummy = "extradata dummy bytes (non-zero-terminated)";
    static const RcOverride kRcOverrideArrayDummy[] =
    {
        {11, -INT_MAX + 3, 10, 3.14f},
        {2, -INT_MAX + 4, 1, 2.71f},
        {3, -INT_MAX + 5, -1, 1.41f},
    };

    quint16 intraMatrixDummy[QnAvCodecHelper::kMatrixLength];
    quint16 interMatrixDummy[QnAvCodecHelper::kMatrixLength];
    for (int i = 0; i < QnAvCodecHelper::kMatrixLength; i++)
    {
        intraMatrixDummy[i] = 65530 + i;
        interMatrixDummy[i] = 57 - i;
    }

    context->codec_id = AV_CODEC_ID_FFMETADATA;
    context->codec_type = AVMEDIA_TYPE_VIDEO;
    QnFfmpegHelper::copyAvCodecContextField((void**) &context->rc_eq, kRcEqDummy,
        strlen(kRcEqDummy) + 1);
    context->extradata_size = (int) strlen(kExtradataDummy);
    QnFfmpegHelper::copyAvCodecContextField((void**) &context->extradata, kExtradataDummy,
        strlen(kExtradataDummy));
    QnFfmpegHelper::copyAvCodecContextField((void**) &context->intra_matrix, intraMatrixDummy,
        sizeof(intraMatrixDummy[0]) * QnAvCodecHelper::kMatrixLength);
    QnFfmpegHelper::copyAvCodecContextField((void**) &context->inter_matrix, interMatrixDummy,
        sizeof(interMatrixDummy[0]) * QnAvCodecHelper::kMatrixLength);
    context->rc_override_count = sizeof(kRcOverrideArrayDummy) / sizeof(kRcOverrideArrayDummy[0]);
    QnFfmpegHelper::copyAvCodecContextField((void**) &context->rc_override, kRcOverrideArrayDummy,
        sizeof(kRcOverrideArrayDummy));

    context->channels = 113;
    context->sample_rate = 114;
    context->sample_fmt = AV_SAMPLE_FMT_NONE;
    context->bits_per_coded_sample = 115;
    context->coded_width = 116;
    context->coded_height = 117;
    context->width = 118;
    context->height = 119;
    context->bit_rate = 120;
    context->channel_layout = ((quint64) INT_MAX) * 10;
    context->block_align = -10;
}

/**
 * Check that the strings are independently allocated and have equal contents, or are both null.
 */
void expectStringsAreDeepCopies(const char* expected, const char* actual, const char* fieldName)
{
    const QByteArray captionString = ("String field: " + QString::fromLatin1(fieldName)).toLatin1();
    const char* const caption = captionString.constData();

    if (expected == nullptr && actual == nullptr)
        return;

    if (expected != nullptr)
        EXPECT_NE(expected, actual) << caption;

    EXPECT_STREQ(expected, actual) << caption;
}

/**
 * Check that the memory regions are independently allocated and have equal contents.
 * Pointers are allowed to be null only if size is 0.
 */
void expectMemsAreDeepCopies(
    const void* expected, const void* actual, int size, const char *fieldName)
{
    const QByteArray captionString = ("Mem field: " + QString::fromLatin1(fieldName)).toLatin1();
    const char* const caption = captionString.constData();

    if (size == 0)
    {
        if (expected == nullptr || actual == nullptr)
        {
            EXPECT_TRUE(expected == nullptr) << caption;
            EXPECT_TRUE(actual == nullptr) << caption;
        }
    }
    else
    {
        ASSERT_TRUE(expected != nullptr) << caption;
        ASSERT_TRUE(actual != nullptr) << caption;
        EXPECT_NE(expected, actual) << caption;
        EXPECT_TRUE(memcmp(expected, actual, size) == 0) << caption;
    }
}

void testRcOverrideFieldsEqual(const RcOverride* expected, const RcOverride* actual, int index)
{
    const QByteArray captionString = ("RcOverride index: " + QString::number(index)).toLatin1();
    const char* const caption = captionString.constData();

    EXPECT_EQ(expected->start_frame, actual->start_frame) << caption;
    EXPECT_EQ(expected->end_frame, actual->end_frame) << caption;
    EXPECT_EQ(expected->qscale, actual->qscale) << caption;
    EXPECT_EQ(expected->quality_factor, actual->quality_factor) << caption;
}

/**
 * Pointers are allowed to be null only if count is 0.
 */
void testRcOverrideArraysAreDeepCopies(
    const RcOverride* expected, const RcOverride* actual, int count)
{
    if (count == 0)
    {
        if (expected == nullptr || actual == nullptr)
        {
            EXPECT_TRUE(expected == nullptr);
            EXPECT_TRUE(actual == nullptr);
        }
    }
    else
    {
        ASSERT_TRUE(expected != nullptr);
        ASSERT_TRUE(actual != nullptr);
        for (int i = 0; i < count; i++)
            testRcOverrideFieldsEqual(&expected[i], &actual[i], i);
    }
}

//-------------------------------------------------------------------------

void testMediaContextFieldsEqual(
    const QnConstMediaContextPtr& expected, const QnConstMediaContextPtr& actual)
{
    EXPECT_EQ(expected->getCodecId(), actual->getCodecId());
    EXPECT_EQ(expected->getCodecType(), actual->getCodecType());
    expectStringsAreDeepCopies(expected->getRcEq(), actual->getRcEq(), "rcEq");

    ASSERT_EQ(expected->getExtradataSize(), actual->getExtradataSize());
    expectMemsAreDeepCopies(expected->getExtradata(), actual->getExtradata(),
        expected->getExtradataSize(), "extradata");

    expectMemsAreDeepCopies(expected->getIntraMatrix(), actual->getIntraMatrix(),
        sizeof(expected->getIntraMatrix()[0]) * QnAvCodecHelper::kMatrixLength, "intraMatrix");
    expectMemsAreDeepCopies(expected->getInterMatrix(), actual->getInterMatrix(),
        sizeof(expected->getInterMatrix()[0]) * QnAvCodecHelper::kMatrixLength, "interMatrix");

    ASSERT_EQ(expected->getRcOverrideCount(), actual->getRcOverrideCount());
    testRcOverrideArraysAreDeepCopies(expected->getRcOverride(), actual->getRcOverride(),
        expected->getRcOverrideCount());

    EXPECT_EQ(expected->getChannels(), actual->getChannels());
    EXPECT_EQ(expected->getSampleRate(), actual->getSampleRate());
    EXPECT_EQ(expected->getSampleFmt(), actual->getSampleFmt());
    EXPECT_EQ(expected->getBitsPerCodedSample(), actual->getBitsPerCodedSample());
    EXPECT_EQ(expected->getCodedWidth(), actual->getCodedWidth());
    EXPECT_EQ(expected->getCodedHeight(), actual->getCodedHeight());
    EXPECT_EQ(expected->getWidth(), actual->getWidth());
    EXPECT_EQ(expected->getHeight(), actual->getHeight());
    EXPECT_EQ(expected->getBitRate(), actual->getBitRate());
    EXPECT_EQ(expected->getChannelLayout(), actual->getChannelLayout());
    EXPECT_EQ(expected->getBlockAlign(), actual->getBlockAlign());
}

void testAvCodecContextFieldsEqual(
    const AVCodecContext* expected, const AVCodecContext* actual)
{
    EXPECT_EQ(expected->codec_id, actual->codec_id);
    EXPECT_EQ(expected->codec_type, actual->codec_type);
    expectStringsAreDeepCopies(expected->rc_eq, actual->rc_eq, "rc_eq");

    ASSERT_EQ(expected->extradata_size, actual->extradata_size);
    expectMemsAreDeepCopies(expected->extradata, actual->extradata,
        expected->extradata_size, "extradata");

    expectMemsAreDeepCopies(expected->intra_matrix, actual->intra_matrix,
        sizeof(expected->intra_matrix[0]) * QnAvCodecHelper::kMatrixLength, "intra_matrix");
    expectMemsAreDeepCopies(expected->inter_matrix, actual->inter_matrix,
        sizeof(expected->inter_matrix[0]) * QnAvCodecHelper::kMatrixLength, "inter_matrix");

    ASSERT_EQ(expected->rc_override_count, actual->rc_override_count);
    testRcOverrideArraysAreDeepCopies(expected->rc_override, actual->rc_override,
        expected->rc_override_count);

    EXPECT_EQ(expected->channels, actual->channels);
    EXPECT_EQ(expected->sample_rate, actual->sample_rate);
    EXPECT_EQ(expected->sample_fmt, actual->sample_fmt);
    EXPECT_EQ(expected->bits_per_coded_sample, actual->bits_per_coded_sample);
    EXPECT_EQ(expected->coded_width, actual->coded_width);
    EXPECT_EQ(expected->coded_height, actual->coded_height);
    EXPECT_EQ(expected->width, actual->width);
    EXPECT_EQ(expected->height, actual->height);
    EXPECT_EQ(expected->bit_rate, actual->bit_rate);
    EXPECT_EQ(expected->channel_layout, actual->channel_layout);
    EXPECT_EQ(expected->block_align, actual->block_align);
}

//-------------------------------------------------------------------------

TEST(QnMediaDataPacket, main)
{
    auto avCodecMediaContext(
        std::make_shared<QnAvCodecMediaContext>(AV_CODEC_ID_NONE));
    fillDummyAvCodecContext(avCodecMediaContext->getAvCodecContext());

    const QByteArray data = avCodecMediaContext->serialize();
    ASSERT_TRUE(data.length() > 0);

    QnConstMediaContextPtr basicMediaContext(
        QnBasicMediaContext::deserialize(data));
    ASSERT_TRUE(basicMediaContext != nullptr);

    testMediaContextFieldsEqual(
        avCodecMediaContext, basicMediaContext);

    AVCodecContext* avCodecContextFromBasic =
        avcodec_alloc_context3(nullptr);
    NX_ASSERT(avCodecContextFromBasic);

    QnFfmpegHelper::mediaContextToAvCodecContext(
        avCodecContextFromBasic, basicMediaContext);

    testAvCodecContextFieldsEqual(
        avCodecMediaContext->getAvCodecContext(), avCodecContextFromBasic);

    auto avCodecMediaContextFromBasic(
        std::make_shared<QnAvCodecMediaContext>(avCodecContextFromBasic));
    testMediaContextFieldsEqual(
        avCodecMediaContext, avCodecMediaContextFromBasic);

    QnFfmpegHelper::deleteAvCodecContext(avCodecContextFromBasic);
}

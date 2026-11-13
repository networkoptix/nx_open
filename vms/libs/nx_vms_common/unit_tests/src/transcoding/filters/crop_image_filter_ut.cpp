// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/media/ffmpeg/frame_info.h>
#include <transcoding/filters/crop_image_filter.h>

namespace transcoding::filters::test {

TEST(CropImageFilter, resultHasRectSize)
{
    CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput());
    ASSERT_TRUE(frame->reallocate(QSize(1280, 720), AV_PIX_FMT_YUV420P));
    for (const bool deepCopy: {true, false})
    {
        QnCropImageFilter filter(QRect(256, 64, 640, 480), /*alignSize*/ false, deepCopy);
        ASSERT_EQ(QSize(640, 480), filter.updateImage(frame)->size()) << "deepCopy " << deepCopy;
    }
}

} // namespace transcoding::filters::test

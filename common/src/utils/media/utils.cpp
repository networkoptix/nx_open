#include "utils.h"
#include <array>

namespace nx {
namespace media {

double getDefaultSampleAspectRatio(const QSize& srcSize)
{
    struct AspectRatioData
    {
        AspectRatioData() {}
        AspectRatioData(const QSize& srcSize, const QSize& dstAspect) :
            srcSize(srcSize),
            dstAspect(dstAspect)
        {
        }

        QSize srcSize;
        QSize dstAspect;
    };

    static std::array<AspectRatioData, 6> kOverridedAr =
    {
        AspectRatioData(QSize(720, 576), QSize(4, 3)),
        AspectRatioData(QSize(720, 480), QSize(4, 3)),
        AspectRatioData(QSize(720, 240), QSize(4, 3)),
        AspectRatioData(QSize(704, 576), QSize(4, 3)),
        AspectRatioData(QSize(704, 480), QSize(4, 3)),
        AspectRatioData(QSize(704, 240), QSize(4, 3))
    };

    for (const auto& data: kOverridedAr)
    {
        if (data.srcSize == srcSize)
        {
            double dstAspect = (double) data.dstAspect.width() / data.dstAspect.height();
            double srcAspect = (double) srcSize.width() / srcSize.height();
            return dstAspect / srcAspect;
        }
    }

    return 1.0;
}

} // namespace media
} // namespace nx

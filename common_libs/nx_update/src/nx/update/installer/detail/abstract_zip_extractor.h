#pragma once

#include <functional>
#include <memory>
#include <QtCore>
#include <utils/update/zip_utils.h>

namespace nx {
namespace update {
namespace detail {

using ExtractHandler = std::function<void(QnZipExtractor::Error)>;

class NX_UPDATE_API AbstractZipExtractor
{
public:
    virtual ~AbstractZipExtractor() = default;
    virtual void extractAsync(
        const QString& filePath,
        const QString& outputDir,
        ExtractHandler extractHandler) = 0;
    virtual void stop() = 0;
};

using AbstractZipExtractorPtr = std::shared_ptr<AbstractZipExtractor>;

} // namespace detail
} // namespace update
} // namespace nx

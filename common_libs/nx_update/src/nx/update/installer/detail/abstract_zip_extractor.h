#pragma once

#include <functional>
#include <memory>
#include <QtCore>
#include <utils/update/zip_utils.h>

namespace nx {
namespace update {
namespace installer {
namespace detail {

using ExtractHandler = std::function<void(QnZipExtractor::Error, const QString& /*outputPath*/)>;

class NX_UPDATE_API AbstractZipExtractor
{
public:
    virtual ~AbstractZipExtractor() = default;
    virtual void extractAsync(
        const QString& filePath,
        const QString& outputDir,
        ExtractHandler extractHandler) = 0;
};

using AbstractZipExtractorPtr = std::shared_ptr<AbstractZipExtractor>;

} // namespace detail
} // namespace installer
} // namespace update
} // namespace nx

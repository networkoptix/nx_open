#pragma once

#include <utils/update/zip_utils.h>

namespace nx {
namespace update {
namespace detail {

using ExtractHandler = std::function<void(QnZipExtractor::Error, const QString& /*outputPath*/)>;

class NX_UPDATE_API ZipExtractor
{
public:
    void extractAsync(
        const QString& filePath,
        const QString& outputDir,
        ExtractHandler extractHandler);
};

} // namespace detail
} // namespace update
} // namespace nx

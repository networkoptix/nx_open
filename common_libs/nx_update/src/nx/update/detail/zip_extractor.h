#pragma once

#include <utils/update/zip_utils.h>

namespace nx::update::detail {

using ExtractHandler = std::function<void(QnZipExtractor::Error, const QString& /*outputPath*/)>;

class ZipExtractor
{
public:
    void extractAsync(
        const QString& filePath,
        const QString& outputDir,
        ExtractHandler extractHandler);
};

} // namespace nx::update::detail

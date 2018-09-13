#include "zip_extractor.h"

#include <memory>

namespace nx::update::detail {

void ZipExtractor::extractAsync(
    const QString& filePath,
    const QString& outputDir,
    ExtractHandler extractHandler)
{
    auto extractor = std::make_shared<QnZipExtractor>(filePath, outputDir);
    QObject::connect(
        extractor.get(),
        &QnZipExtractor::finished,
        [extractor, extractHandler](int error)
        {
            extractHandler(
                static_cast<QnZipExtractor::Error>(error),
                extractor->dir().absolutePath());
        });
    extractor->start();
}

} // namespace nx::update::detail

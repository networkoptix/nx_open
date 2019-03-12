#include "zip_extractor.h"

#include <memory>

namespace nx::update::detail {

void ZipExtractor::extractAsync(
    const QString& filePath,
    const QString& outputDir,
    ExtractHandler extractHandler)
{
    m_extractor = std::make_unique<QnZipExtractor>(filePath, outputDir);
    QObject::connect(
        m_extractor.get(),
        &QnZipExtractor::finished,
        [this, extractHandler](int error)
        {
            extractHandler(
                static_cast<QnZipExtractor::Error>(error),
                m_extractor->dir().absolutePath());
        });
    m_extractor->start();
}

} // namespace nx::update::detail

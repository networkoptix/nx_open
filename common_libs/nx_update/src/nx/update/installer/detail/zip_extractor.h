#pragma once

#include <nx/update/installer/detail/abstract_zip_extractor.h>

namespace nx {
namespace update {
namespace installer {
namespace detail {

class NX_UPDATE_API ZipExtractor: public AbstractZipExtractor
{
public:
    virtual void extractAsync(
        const QString& filePath,
        const QString& outputDir,
        ExtractHandler extractHandler) override;
};

} // namespace detail
} // namespace installer
} // namespace update
} // namespace nx

#pragma once

#include <QtCore/QString>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

class AbstractAsyncRawDataProvider
{
public:
    virtual ~AbstractAsyncRawDataProvider() {}
    virtual void getUpdatesMetaInformation() = 0;
    virtual void getSpecificUpdateData(const QString& updatePrefix, const QString& build) = 0;
};

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx

#pragma once

#include <vector>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test_support {

struct UpdateTestData
{
    QString customization;
    QString version;
    QByteArray json;
};

NX_UPDATE_API const QByteArray& metaDataJson();
NX_UPDATE_API const std::vector<UpdateTestData>& updateTestDataList();

} // namespace test_support
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx

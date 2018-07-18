#pragma once

#include <vector>
#include <QtCore/QString>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test_support {

struct UpdateTestData
{
    QString updatePrefix;
    QString build;
    QByteArray json;

    QString customization() const { return updatePrefix.mid(updatePrefix.lastIndexOf("/") + 1);}
};

NX_UPDATE_API const QByteArray& metaDataJson();
NX_UPDATE_API const std::vector<UpdateTestData>& updateTestDataList();

} // namespace test_support
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx

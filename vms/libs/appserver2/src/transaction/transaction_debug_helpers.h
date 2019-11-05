#pragma once

#include <nx/vms/api/data/media_server_data.h>

#include <QtCore/QString>

#include <vector>
#include <type_traits>

namespace ec2 {

template<typename Data, typename = std::void_t<>>
struct HasId : std::false_type { };

template<typename Data>
struct HasId<Data, std::void_t<decltype(Data().id)>> : std::true_type { };

template<typename Data>
constexpr bool kHasId = HasId<Data>::value;

/**
 * Minimal transaction data debugging information functions. Only general information is returned
 * not to disclose sensitive user data.
 */
QString debugString(const nx::vms::api::StorageData& data);

template<typename RequestData>
QString debugString(const RequestData& data);

template<typename RequestData>
QString debugString(const std::vector<RequestData>& dataList)
{
    QString result = "[";
    for (const auto& entry: dataList)
        result += debugString(entry) + ", ";

    return (dataList.empty() ? result : result.left(result.size() - 2)) + "]";
}

template<typename RequestData>
QString debugString(const RequestData& data)
{
    if constexpr (kHasId<RequestData>)
        return QString("IdData: id: %1").arg(data.id.toString());
    else
        return QString(typeid(RequestData).name());
}

} // namespace ec2

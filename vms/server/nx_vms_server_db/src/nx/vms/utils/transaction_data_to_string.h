#pragma once

#include <nx/vms/api/data/media_server_data.h>

#include <QtCore/QString>

#include <vector>
#include <type_traits>

namespace nx::vms::utils {

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
QString toString(const nx::vms::api::StorageData& data);

template<typename RequestData>
QString toString(const RequestData& data);

template<typename RequestData>
QString toString(const std::vector<RequestData>& dataList)
{
    QString result = "[";
    for (const auto& entry: dataList)
        result += toString(entry) + ", ";

    return (dataList.empty() ? result : result.left(result.size() - 2)) + "]";
}

template<typename RequestData>
QString toString(const RequestData& data)
{
    if constexpr (kHasId<RequestData>)
        return QString("IdData: id: %1").arg(data.id.toString());
    else
        return QString(typeid(RequestData).name());
}

} // namespace ec2

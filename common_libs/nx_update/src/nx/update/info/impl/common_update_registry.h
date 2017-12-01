#pragma once

#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/data_parser/update_data.h>

namespace nx {
namespace update {
namespace info {
namespace impl {

class CommonUpdateRegistry: public AbstractUpdateRegistry
{
public:
    CommonUpdateRegistry(
        detail::data_parser::UpdatesMetaData metaData,
        QList<detail::data_parser::UpdateData> updateDataList);
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx

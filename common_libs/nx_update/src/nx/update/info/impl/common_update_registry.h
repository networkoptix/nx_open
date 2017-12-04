#pragma once

#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/data_parser/update_data.h>
#include <nx/update/info/detail/customization_version_data.h>

namespace nx {
namespace update {
namespace info {
namespace impl {

class CommonUpdateRegistry: public AbstractUpdateRegistry
{
public:
    CommonUpdateRegistry(
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate);
    virtual ResultCode findUpdate(
        const UpdateRequestData& updateRequestData,
        FileData* outFileData) override;
    virtual QList<QString> alternativeServers() const override;
private:
    detail::data_parser::UpdatesMetaData m_metaData;
    detail::CustomizationVersionToUpdate m_customizationVersionToUpdate;

    bool hasCustomization(const QString& customization);
    bool hasUpdateForOs(const OsVersion& osVersion);
    ResultCode hasUpdateForVersionAndCloudHost(const UpdateRequestData& updateRequestData);
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx

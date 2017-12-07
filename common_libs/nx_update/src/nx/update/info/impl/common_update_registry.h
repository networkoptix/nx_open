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
        const QString& baseUrl,
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate);
    virtual ResultCode findUpdate(
        const UpdateRequestData& updateRequestData,
        FileData* outFileData) override;
    virtual QList<QString> alternativeServers() const override;
    virtual QByteArray toByteArray() const override;
    virtual bool fromByteArray(const QByteArray& rawData) override;
private:
    QString m_baseUrl;
    detail::data_parser::UpdatesMetaData m_metaData;
    detail::CustomizationVersionToUpdate m_customizationVersionToUpdate;

    bool hasUpdateForCustomizationAndVersion(
        const UpdateRequestData& updateRequestData,
        detail::data_parser::CustomizationData* customizationData);
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx

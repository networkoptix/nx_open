#pragma once

#include <nx/update/info/abstract_update_registry.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
#include <nx/update/info/detail/data_parser/update_data.h>
#include <nx/update/info/detail/customization_version_data.h>

namespace nx {
namespace update {
namespace info {
namespace impl {

class NX_UPDATE_API CommonUpdateRegistry: public AbstractUpdateRegistry
{
public:
    CommonUpdateRegistry(
        const QString& baseUrl,
        detail::data_parser::UpdatesMetaData metaData,
        detail::CustomizationVersionToUpdate customizationVersionToUpdate);

    CommonUpdateRegistry() = default;
    virtual ResultCode findUpdateFile(
        const UpdateFileRequestData& updateFileRequestData,
        FileData* outFileData) const override;

    virtual ResultCode latestUpdate(
        const UpdateRequestData& updateRequestData,
        QnSoftwareVersion* outSoftwareVersion) const override;

    virtual void addFileData(const ManualFileData& manualFileData) override;
    virtual QList<QString> alternativeServers() const override;
    virtual QByteArray toByteArray() const override;
    virtual bool fromByteArray(const QByteArray& rawData) override;
    virtual bool equals(AbstractUpdateRegistry* other) const override;
    virtual void merge(AbstractUpdateRegistry* other) override;
    virtual QList<QnUuid> additionalPeers(const QString& fileName) const override;

private:
    QString m_baseUrl;
    detail::data_parser::UpdatesMetaData m_metaData;
    detail::CustomizationVersionToUpdate m_customizationVersionToUpdate;
    QList<ManualFileData> m_manualData;

    bool hasUpdateForCustomizationAndVersion(
        const UpdateRequestData& updateRequestData,
        detail::data_parser::CustomizationData* customizationData) const;
};

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx

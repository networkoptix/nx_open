#pragma once

#include <QtCore/QUrlQuery>

#include <chrono>
#include <string>
#include <vector>

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/cdb/client/data/system_data.h>

namespace nx {
namespace cdb {
namespace api {

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemSharingEx)(SystemHealthHistoryItem)(SystemHealthHistory),
    (sql_record));

} // namespace api

namespace data {

/**
 * Information required to register system in cloud.
 */
class SystemRegistrationData:
    public api::SystemRegistrationData,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class SystemRegistrationDataWithAccount:
    public SystemRegistrationData
{
public:
    std::string accountEmail;

    SystemRegistrationDataWithAccount(SystemRegistrationData&& right):
        SystemRegistrationData(std::move(right))
    {
    }
};

class SystemData:
    public api::SystemData,
    public nx::utils::stree::AbstractResourceReader
{
public:
    /** Seconds since epoch (1970-01-01). */
    int expirationTimeUtc;
    bool activationInDbNeeded;

    SystemData();

    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

#define SystemData_FieldsEx SystemData_Fields (expirationTimeUtc)


class SystemDataList:
    public api::SystemDataList
{
public:
};

class SystemSharing:
    public api::SystemSharing,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

#define SystemSharing_FieldsEx SystemSharing_Fields (vmsUserId)

class SystemSharingList:
    public nx::utils::stree::AbstractResourceReader
{
public:
    std::vector<SystemSharing> sharing;

    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

bool loadFromUrlQuery(const QUrlQuery& urlQuery, SystemSharingList* const systemSharing);

/**
 * For requests passing just system id.
 */
class SystemId:
    public api::SystemId,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class SystemAttributesUpdate:
    public api::SystemAttributesUpdate,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class UserSessionDescriptor:
    public api::UserSessionDescriptor,
    public nx::utils::stree::AbstractResourceReader
{
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemRegistrationData)(SystemData)(SystemSharing)(SystemId),
    (sql_record));

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (SystemSharingList),
    (json));

} // namespace data
} // namespace cdb
} // namespace nx

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <variant>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/latin1_array.h>
#include <nx/utils/qnbytearrayref.h>
#include <nx/utils/software_version.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>

struct NX_VMS_COMMON_API PeriodDetails
{
    /**%apidoc Period start time in seconds. */
    std::chrono::seconds startS{0};
    /**%apidoc Period end time in seconds. */
    std::chrono::seconds endS{0};
    bool operator==(const PeriodDetails& other) const = default;
};
#define PeriodDetails_Fields (startS)(endS)

struct NX_VMS_COMMON_API IdDetails
{
    /**%apidoc List of resource identificators. */
    std::vector<nx::Uuid> ids;
    bool operator==(const IdDetails& other) const = default;
};
#define IdDetails_Fields (ids)

struct NX_VMS_COMMON_API DescriptionDetails
{
    /**%apidoc Additional description of the Audit Record. */
    QString description;
    bool operator==(const DescriptionDetails& other) const = default;
};
#define DescriptionDetails_Fields (description)
QN_FUSION_DECLARE_FUNCTIONS(DescriptionDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(DescriptionDetails, DescriptionDetails_Fields)

struct NX_VMS_COMMON_API SessionDetails: PeriodDetails
{
    nx::network::http::AuthMethod::Value authMethod = nx::network::http::AuthMethod::notDefined;
    bool operator==(const SessionDetails& other) const = default;
};
#define SessionDetails_Fields PeriodDetails_Fields (authMethod)
QN_FUSION_DECLARE_FUNCTIONS(SessionDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(SessionDetails, SessionDetails_Fields)

struct NX_VMS_COMMON_API PlaybackDetails: IdDetails, PeriodDetails
{
    /**%apidoc
     * List containing a flag for each Device in the list of ids
     * that indicates if an archive exists for the specified time period.
     */
    std::map<nx::Uuid, bool> archiveExists;
    bool operator==(const PlaybackDetails& other) const = default;
};
#define PlaybackDetails_Fields IdDetails_Fields PeriodDetails_Fields (archiveExists)
QN_FUSION_DECLARE_FUNCTIONS(PlaybackDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(PlaybackDetails, PlaybackDetails_Fields)

struct NX_VMS_COMMON_API ResourceDetails: IdDetails, DescriptionDetails
{
    bool operator==(const ResourceDetails& other) const = default;
};
#define ResourceDetails_Fields IdDetails_Fields DescriptionDetails_Fields
QN_FUSION_DECLARE_FUNCTIONS(ResourceDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(ResourceDetails, ResourceDetails_Fields)

struct NX_VMS_COMMON_API DeviceDetails: ResourceDetails
{
    /**%apidoc List of Device MACs. */
    std::map<nx::Uuid, QnLatin1Array> macs;
    bool operator==(const DeviceDetails& other) const = default;
};
#define DeviceDetails_Fields ResourceDetails_Fields(macs)
QN_FUSION_DECLARE_FUNCTIONS(DeviceDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(DeviceDetails, DeviceDetails_Fields)

struct NX_VMS_COMMON_API MitmDetails
{
    /**%apidoc Id of the compromised Server. */
    nx::Uuid id;
    /**%apidoc Server expected certificate in PEM format. */
    QByteArray expectedPem;
    /**%apidoc Server actual certificate in PEM format. */
    QByteArray actualPem;
    /**%apidoc Requested Url on the compromised Server. */
    nx::utils::Url url;
    bool operator==(const MitmDetails& other) const = default;
};
#define MitmDetails_Fields (id)(expectedPem)(actualPem)(url)
QN_FUSION_DECLARE_FUNCTIONS(MitmDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(MitmDetails, MitmDetails_Fields)

struct NX_VMS_COMMON_API UpdateDetails
{
    /**%apidoc Version of the installed update. */
    nx::utils::SoftwareVersion version;
    bool operator==(const UpdateDetails& other) const = default;
};
#define UpdateDetails_Fields (version)
QN_FUSION_DECLARE_FUNCTIONS(UpdateDetails, (ubjson)(json), NX_VMS_COMMON_API)
NX_REFLECTION_INSTRUMENT(UpdateDetails, UpdateDetails_Fields)

namespace details
{
    template<nx::vms::api::AuditRecordType type, typename Details>
    struct map
    {
        static constexpr nx::vms::api::AuditRecordType key = type;
        using value = Details;
    };

    template <typename Variant, typename... Types>
    struct unique
    {
        using type = Variant;
    };

    template <template<typename...> class Variant, typename... Details, typename ToCheck, typename... Rest>
    struct unique<Variant<Details...>, ToCheck, Rest...>
        :std::conditional<std::disjunction<std::is_same<typename ToCheck::value, Details> ... >::value,
            unique<Variant<Details...>, Rest...>,
            unique<Variant<Details..., typename ToCheck::value>, Rest...>
        >::type
    {};

    template<nx::vms::api::AuditRecordType type, typename Mapping, std::size_t idx = 0>
    struct details_type;

    template<nx::vms::api::AuditRecordType type, typename Mapping, std::size_t idx>
    struct details_type<type, std::tuple<Mapping>, idx>
        :std::conditional<Mapping::key == type,
            std::type_identity<typename Mapping::value>,
            std::type_identity<void>
        >::type
    {};

    template<nx::vms::api::AuditRecordType type, typename Mapping, typename... Mappings, std::size_t idx>
    struct details_type<type, std::tuple<Mapping, Mappings...>, idx>
        :std::conditional<Mapping::key == type,
            std::type_identity<typename Mapping::value>,
            details_type<type, std::tuple<Mappings...>, idx+1>
        >::type
    {};

    template<typename T, typename Variant, std::size_t idx>
    struct find_exact_type_index;

    template<typename T,
        template<typename...> typename Variant,
        std::size_t idx, typename U, typename... Types>
    struct find_exact_type_index<T, Variant<U, Types...>, idx>
        :std::conditional<std::is_same<T, U>::value,
            std::integral_constant<std::size_t, idx>,
            find_exact_type_index<T, Variant<Types...>, idx+1>
        >::type
    {};

    template<typename T, typename U, typename Variant>
    T* get_if_base(Variant* variant)
    {
        if constexpr (std::disjunction<std::is_same<T, U>, std::is_base_of<T, U>>::value)
            return std::get_if<U>(variant);
        else
            return nullptr;
    }

    template<typename T, typename... Details>
    T* get_if(std::variant<Details...>* variant)
    {
        T* r = nullptr;
        ((r = get_if_base<T, Details>(variant)) || ...);
        return r;
    }

    template<typename T, typename... Details>
    const T* get_if(const std::variant<Details...>* variant)
    {
        const T* r = nullptr;
        ((r = get_if_base<const T, Details>(variant)) || ...);
        return r;
    }

    template<typename... Mappings>
    struct audit_type_map_to_details
    {
        using type = typename unique<std::variant<>, Mappings...>::type;
        using mapping = std::tuple<Mappings...>;
    };

} // namespace details

using AllAuditDetails =
    details::audit_type_map_to_details<
        details::map<nx::vms::api::AuditRecordType::notDefined, std::nullptr_t>,
        details::map<nx::vms::api::AuditRecordType::cameraInsert, DeviceDetails>,
        details::map<nx::vms::api::AuditRecordType::cameraUpdate, DeviceDetails>,
        details::map<nx::vms::api::AuditRecordType::cameraRemove, DeviceDetails>,
        details::map<nx::vms::api::AuditRecordType::viewLive, PlaybackDetails>,
        details::map<nx::vms::api::AuditRecordType::viewArchive, PlaybackDetails>,
        details::map<nx::vms::api::AuditRecordType::exportVideo, PlaybackDetails>,
        details::map<nx::vms::api::AuditRecordType::unauthorizedLogin, SessionDetails>,
        details::map<nx::vms::api::AuditRecordType::login, SessionDetails>,
        details::map<nx::vms::api::AuditRecordType::userUpdate, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::userRemove, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::serverUpdate, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::serverRemove, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::eventUpdate, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::eventRemove, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::storageInsert, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::storageUpdate, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::storageRemove, ResourceDetails>,
        details::map<nx::vms::api::AuditRecordType::systemNameChanged, DescriptionDetails>,
        details::map<nx::vms::api::AuditRecordType::settingsChange, DescriptionDetails>,
        details::map<nx::vms::api::AuditRecordType::emailSettings, DescriptionDetails>,
        details::map<nx::vms::api::AuditRecordType::systemmMerge, std::nullptr_t>,
        details::map<nx::vms::api::AuditRecordType::eventReset, std::nullptr_t>,
        details::map<nx::vms::api::AuditRecordType::databaseRestore, std::nullptr_t>,
        details::map<nx::vms::api::AuditRecordType::updateInstall, UpdateDetails>,
        details::map<nx::vms::api::AuditRecordType::mitmAttack, MitmDetails>,
        details::map<nx::vms::api::AuditRecordType::cloudBind, std::nullptr_t>,
        details::map<nx::vms::api::AuditRecordType::cloudUnbind, std::nullptr_t>>;

void serialize_field(const AllAuditDetails::type& data, QVariant* target);
void deserialize_field(const QVariant& value, AllAuditDetails::type* target);

void serialize(const AllAuditDetails::type& data, QnUbjsonWriter<QByteArray>* stream);
bool deserialize(QnUbjsonReader<QByteArray>* stream, AllAuditDetails::type* target);

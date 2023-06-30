// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audit_record.h"

#include <nx/fusion/model_functions.h>
#include <utils/common/synctime.h>

template<nx::vms::api::AuditRecordType recordType>
struct Conversion
{
    static constexpr nx::vms::api::AuditRecordType type = recordType;

    static constexpr auto verifyDetailsToParams =
        [](auto F, const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
        {
            const auto details = record.get<typename details::details_type<recordType, AllAuditDetails::mapping>::type>();
            NX_ASSERT(details);

            return F(record, legacyRecord);
        };

    static constexpr auto verifyParamsToDetails =
        [](auto F, const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
        {
            F(legacyRecord, record);

            const auto details = record.get<typename details::details_type<recordType, AllAuditDetails::mapping>::type>();
            NX_ASSERT(details);
        };

    std::function<void(const QnAuditRecord&, QnLegacyAuditRecord&)> detailsToParams;
    std::function<void(const QnLegacyAuditRecord&, QnAuditRecord&)> paramsToDetails;
};

namespace detailsToParams {

const auto session =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<SessionDetails>();

        legacyRecord.rangeStartSec = details->startS.count();
        legacyRecord.rangeEndSec = details->endS.count();
    };
const auto playback =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<PlaybackDetails>();

        legacyRecord.rangeStartSec = details->startS.count();
        legacyRecord.rangeEndSec = details->endS.count();
        legacyRecord.resources = details->ids;

        QByteArray archiveExist;
        for (const auto& id: legacyRecord.resources)
        {
            const auto it = details->archiveExists.find(id);
            bool exists = (it != details->archiveExists.end()) && it->second;
            archiveExist.append(exists ? '1' : '0');
        }
        legacyRecord.addParam("archiveExist", archiveExist);
    };
const auto device =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<DeviceDetails>();

        legacyRecord.resources = details->ids;
        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto resource =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<ResourceDetails>();

        legacyRecord.resources = details->ids;
        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto description =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<DescriptionDetails>();

        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto cloud =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<CloudDetails>();

        if (!details->userAgent.isEmpty())
            legacyRecord.addParam("userAgent", details->userAgent.toLatin1());
    };
const auto nothing =
    [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<std::nullptr_t>();
    };

} // namespace detailsToParams

namespace paramsToDetails {

const auto session =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = SessionDetails{std::chrono::seconds(legacyRecord.rangeStartSec),
            std::chrono::seconds(legacyRecord.rangeEndSec)};
    };
const auto playback =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        const auto flags = QByteArray(legacyRecord.extractParam("archiveExist"));
        std::map<nx::Uuid, bool> archiveExists;
        size_t i = 0;
        for (const auto& id: legacyRecord.resources)
        {
            bool exists = flags.size() > i && flags[i] == '1';
            archiveExists.insert({id, exists});
            ++i;
        }

        record.details = PlaybackDetails{
            {legacyRecord.resources},
            {std::chrono::seconds(legacyRecord.rangeStartSec), std::chrono::seconds(legacyRecord.rangeEndSec)},
            archiveExists};
    };
const auto device =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = DeviceDetails{{
            {legacyRecord.resources},
            {QByteArray(legacyRecord.extractParam("description"))}}};
    };
const auto resource =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = ResourceDetails{
            {legacyRecord.resources},
            {QByteArray(legacyRecord.extractParam("description"))}};
    };
const auto description =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = DescriptionDetails{QByteArray(legacyRecord.extractParam("description"))};
    };
const auto cloud =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = CloudDetails{QByteArray(legacyRecord.extractParam("userAgent"))};
    };
const auto nothing =
    [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
    {
        record.details = nullptr;
    };

} // namespace paramsToDetails

template<typename Visitor>
constexpr auto visitAllConversions(Visitor&& visitor)
{
    using nx::vms::api::AuditRecordType;
    return visitor(
        Conversion<AuditRecordType::unauthorizedLogin>{detailsToParams::session, paramsToDetails::session},
        Conversion<AuditRecordType::login>{detailsToParams::session, paramsToDetails::session},
        Conversion<AuditRecordType::viewArchive>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::viewLive>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::exportVideo>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::cameraInsert>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::cameraUpdate>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::cameraRemove>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::userUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::userRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageInsert>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::eventUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::eventRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::eventReset>{detailsToParams::nothing, paramsToDetails::nothing},
        Conversion<AuditRecordType::serverUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::serverRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::systemNameChanged>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::settingsChange>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::emailSettings>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::mitmAttack>{
            [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
            {
                const auto details = record.get<MitmDetails>();

                if (!details->id.isNull())
                    legacyRecord.addParam("serverId", details->id.toByteArray());
                if (!details->expectedPem.isEmpty())
                    legacyRecord.addParam("expectedPem", details->expectedPem);
                if (!details->actualPem.isEmpty())
                    legacyRecord.addParam("actualPem", details->actualPem);
                if (!details->url.isEmpty())
                    legacyRecord.addParam("url", details->url.toString().toLatin1());
            },
            [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
            {
                record.details = MitmDetails{
                    nx::Uuid(QByteArray(legacyRecord.extractParam("serverId"))),
                    QByteArray(legacyRecord.extractParam("expectedPem")),
                    QByteArray(legacyRecord.extractParam("actualPem")),
                    nx::utils::Url(legacyRecord.extractParam("url"))};
            }},
        Conversion<AuditRecordType::updateInstall>{
            [](const QnAuditRecord& record, QnLegacyAuditRecord& legacyRecord)
            {
                const auto details = record.get<UpdateDetails>();

                if (!details->version.isNull())
                    legacyRecord.addParam("version", details->version.toString().toLatin1());
            },
            [](const QnLegacyAuditRecord& legacyRecord, QnAuditRecord& record)
            {
                record.details = UpdateDetails{nx::utils::SoftwareVersion(
                    QByteArray(legacyRecord.extractParam("version")))};
            }},
        Conversion<AuditRecordType::cloudBind>{detailsToParams::cloud, paramsToDetails::cloud},
        Conversion<AuditRecordType::cloudUnbind>{detailsToParams::cloud, paramsToDetails::cloud},
        Conversion<AuditRecordType::systemmMerge>{detailsToParams::nothing, paramsToDetails::nothing},
        Conversion<AuditRecordType::databaseRestore>{detailsToParams::nothing, paramsToDetails::nothing}
    );
}

template<typename Func, typename... Args>
auto switchByRecordType(nx::vms::api::AuditRecordType eventType, Func&& f, Args&&... args)
{
    const auto success = (((eventType == args.type) && (f(std::forward<Args>(args)), true)) || ...);
    NX_ASSERT(success);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnLegacyAuditRecord, (ubjson)(json)(sql_record), QnLegacyAuditRecord_Fields)

void QnLegacyAuditRecord::removeParam(const QnLatin1Array& name)
{
    int pos = params.indexOf(name);
    if (pos == -1)
        return;
    int pos2 = params.indexOf(pos, ';');
    if (pos2 == -1)
        pos2 = params.size();
    if (pos > 0)
        --pos;
    params.remove(pos, pos2 - pos);
}

QnByteArrayConstRef QnLegacyAuditRecord::extractParam(const QnLatin1Array& name) const
{
    int pos = params.indexOf(name);
    if (pos == -1)
        return QnByteArrayConstRef();
    int pos2 = params.indexOf(';', pos);
    if (pos2 == -1)
        pos2 = params.size();
    pos += name.size() + 1;
    return QnByteArrayConstRef(params, pos, pos2 - pos);
}
void QnLegacyAuditRecord::addParam(const QnLatin1Array& name, const QnLatin1Array& value)
{
    if (!params.isEmpty()) {
        removeParam(name);
        if (!params.isEmpty())
            params.append(';');
    }
    params.append(name).append('=').append(value);
}

QnLegacyAuditRecord::operator QnAuditRecord() const
{
    return visitAllConversions(
        [this](auto&&... items)
        {
            ::QnAuditRecord record;
            record.eventType = static_cast<nx::vms::api::AuditRecordType>(eventType);
            record.createdTimeS = std::chrono::seconds(createdTimeSec);
            record.authSession = authSession;
            switchByRecordType(record.eventType,
                [this, &record](auto&& conversion)
                {
                    conversion.verifyParamsToDetails(conversion.paramsToDetails, *this, record);
                },
                items...);
            return record;
        });
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnAuthSession, (ubjson)(json)(sql_record), QnAuthSession_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    QnAuditRecord, (ubjson)(json), QnAuditRecord_Fields)

QN_FUSION_ADAPT_CLASS(QnAuditRecordModel,
    ((getter, &QnAuditRecordModel::eventType)(setter, &QnAuditRecordModel::eventType)(name, "eventType"))
    ((getter, &QnAuditRecordModel::details)(setter, &QnAuditRecordModel::details)(name, "details"))
    ((getter, &QnAuditRecordModel::createdTimeS)(setter, &QnAuditRecordModel::createdTimeS)(name, "createdTimeSec"))
    ((getter, &QnAuditRecordModel::getRangeStartSec)(setter, &QnAuditRecordModel::setRangeStartSec)(name, "rangeStartSec"))
    ((getter, &QnAuditRecordModel::getRangeEndSec)(setter, &QnAuditRecordModel::setRangeEndSec)(name, "rangeEndSec"))
    ((getter, &QnAuditRecordModel::authSession)(setter, &QnAuditRecordModel::authSession)(name, "authSession")))
QN_FUSION_DEFINE_FUNCTIONS(QnAuditRecordModel, (sql_record))

QnAuditRecordModel::QnAuditRecordModel(const QnAuditRecord& record)
    :QnAuditRecord(record)
{}

void QnAuditRecordModel::setRangeStartSec(int value)
{
    auto details = get<PeriodDetails>();
    if (details)
        details->startS = std::chrono::seconds(value);
}

void QnAuditRecordModel::setRangeEndSec(int value)
{
    auto details = get<PeriodDetails>();
    if (details)
        details->endS = std::chrono::seconds(value);
}

int QnAuditRecordModel::getRangeStartSec() const
{
    auto details = get<PeriodDetails>();
    if (details)
        return details->startS.count();
    return 0;
}

int QnAuditRecordModel::getRangeEndSec() const
{
    auto details = get<PeriodDetails>();
    if (details)
        return details->endS.count();
    return 0;
}

QnAuditRecord::operator QnLegacyAuditRecord() const
{
    return visitAllConversions(
        [this](auto&&... items)
        {
            QnLegacyAuditRecord record;
            record.eventType = static_cast<Qn::LegacyAuditRecordType>(eventType);
            record.createdTimeSec = createdTimeS.count();
            record.authSession = authSession;
            switchByRecordType(eventType,
                [this, &record](auto&& conversion)
                {
                    conversion.verifyDetailsToParams(conversion.detailsToParams, *this, record);
                },
                items...);
            return record;
        });
}

QnAuditRecord QnAuditRecord::prepareRecord(nx::vms::api::AuditRecordType type, const QnAuthSession& authInfo)
{
    QnAuditRecord result;
    result.eventType = type;
    result.authSession = authInfo;
    result.createdTimeS = std::chrono::seconds(qnSyncTime->currentMSecsSinceEpoch() / 1000);
    return result;
}

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "legacy_audit_record.h"

#include <nx/fusion/model_functions.h>
#include <utils/common/synctime.h>

namespace {

using namespace nx::vms::api;

template<AuditRecordType recordType>
struct Conversion
{
    static constexpr AuditRecordType type = recordType;

    static constexpr auto verifyDetailsToParams =
        [](auto F, const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
        {
            return F(record, legacyRecord);
        };

    static constexpr auto verifyParamsToDetails =
        [](auto F, const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
        {
            F(legacyRecord, record);

            const auto details =
                record.get<typename details::details_type<type, AllAuditDetails::mapping>::type>();
            NX_ASSERT(details);
        };

    std::function<void(const AuditRecord&, QnLegacyAuditRecord&)> detailsToParams;
    std::function<void(const QnLegacyAuditRecord&, AuditRecord&)> paramsToDetails;
};

namespace detailsToParams {

const auto session =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<SessionDetails>();

        legacyRecord.rangeStartSec = details->startS.count();
        legacyRecord.rangeEndSec = details->endS.count();
    };
const auto proxySession =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<ProxySessionDetails>();

        legacyRecord.rangeStartSec = details->startS.count();
        legacyRecord.rangeEndSec = details->endS.count();
        legacyRecord.addParam("target", details->target.toUtf8());
        legacyRecord.addParam("wasConnected", details->wasConnected ? "1": "0");
    };
const auto playback =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
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
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<DeviceDetails>();

        legacyRecord.resources = details->ids;
        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto resource =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<ResourceDetails>();

        legacyRecord.resources = details->ids;
        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto description =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        const auto details = record.get<DescriptionDetails>();

        if (!details->description.isEmpty())
            legacyRecord.addParam("description", details->description.toLatin1());
    };
const auto cloud =
    [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
    {
        legacyRecord.addParam("userAgent", record.authSession.userAgent.toUtf8());
    };
const auto nothing =
    [](const AuditRecord& record, QnLegacyAuditRecord& /*legacyRecord*/)
    {
        const auto details = record.get<std::nullptr_t>();
    };

} // namespace detailsToParams

namespace paramsToDetails {

const auto session =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        record.details = SessionDetails{std::chrono::seconds(legacyRecord.rangeStartSec),
            std::chrono::seconds(legacyRecord.rangeEndSec)};
    };
const auto proxySession =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        record.details = ProxySessionDetails{
            {std::chrono::seconds(legacyRecord.rangeStartSec), std::chrono::seconds(legacyRecord.rangeEndSec)},
            QString(legacyRecord.extractParam("target")),
            legacyRecord.extractParam("wasConnected") == "1"};
    };
const auto playback =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        const auto flags = QByteArray(legacyRecord.extractParam("archiveExist"));
        std::map<nx::Uuid, bool> archiveExists;
        int i = 0;
        for (const auto& id: legacyRecord.resources)
        {
            bool exists = (int) flags.size() > i && flags[i] == '1';
            archiveExists.insert({id, exists});
            ++i;
        }

        record.details = PlaybackDetails{
            {legacyRecord.resources},
            {std::chrono::seconds(legacyRecord.rangeStartSec), std::chrono::seconds(legacyRecord.rangeEndSec)},
            archiveExists};
    };
const auto device =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        record.details = DeviceDetails{{
            {legacyRecord.resources},
            {QByteArray(legacyRecord.extractParam("description"))}}};
    };
const auto resource =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        record.details = ResourceDetails{
            {legacyRecord.resources},
            {QByteArray(legacyRecord.extractParam("description"))}};
    };
const auto description =
    [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
    {
        record.details = DescriptionDetails{QByteArray(legacyRecord.extractParam("description"))};
    };
const auto cloud =
    [](const QnLegacyAuditRecord& /*legacyRecord*/, AuditRecord& record)
    {
        record.details = nullptr;
    };
const auto nothing =
    [](const QnLegacyAuditRecord& /*legacyRecord*/, AuditRecord& record)
    {
        record.details = nullptr;
    };

} // namespace paramsToDetails

template<typename Visitor>
constexpr auto visitAllConversions(Visitor&& visitor)
{
    return visitor(
        Conversion<AuditRecordType::unauthorizedLogin>{detailsToParams::session, paramsToDetails::session},
        Conversion<AuditRecordType::login>{detailsToParams::session, paramsToDetails::session},
        Conversion<AuditRecordType::viewArchive>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::viewLive>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::exportVideo>{detailsToParams::playback, paramsToDetails::playback},
        Conversion<AuditRecordType::deviceInsert>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::deviceUpdate>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::deviceRemove>{detailsToParams::device, paramsToDetails::device},
        Conversion<AuditRecordType::userUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::userRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageInsert>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::storageRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::eventUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::eventRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::serverUpdate>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::serverRemove>{detailsToParams::resource, paramsToDetails::resource},
        Conversion<AuditRecordType::siteNameChanged>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::settingsChange>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::emailSettings>{detailsToParams::description, paramsToDetails::description},
        Conversion<AuditRecordType::mitmAttack>{
            [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
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
            [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
            {
                record.details = MitmDetails{
                    nx::Uuid(QByteArray(legacyRecord.extractParam("serverId"))),
                    QByteArray(legacyRecord.extractParam("expectedPem")),
                    QByteArray(legacyRecord.extractParam("actualPem")),
                    nx::utils::Url(legacyRecord.extractParam("url"))};
            }},
        Conversion<AuditRecordType::updateInstall>{
            [](const AuditRecord& record, QnLegacyAuditRecord& legacyRecord)
            {
                const auto details = record.get<UpdateDetails>();

                if (!details->version.isNull())
                    legacyRecord.addParam("version", details->version.toString().toLatin1());
            },
            [](const QnLegacyAuditRecord& legacyRecord, AuditRecord& record)
            {
                record.details = UpdateDetails{nx::utils::SoftwareVersion(
                    QByteArray(legacyRecord.extractParam("version")))};
            }},
        Conversion<AuditRecordType::cloudBind>{detailsToParams::cloud, paramsToDetails::cloud},
        Conversion<AuditRecordType::cloudUnbind>{detailsToParams::cloud, paramsToDetails::cloud},
        Conversion<AuditRecordType::siteMerge>{detailsToParams::nothing, paramsToDetails::nothing},
        Conversion<AuditRecordType::databaseRestore>{detailsToParams::nothing, paramsToDetails::nothing},
        Conversion<AuditRecordType::proxySession>{detailsToParams::proxySession, paramsToDetails::proxySession}
    );
}

template<typename Func, typename... Args>
auto switchByRecordType(AuditRecordType eventType, Func&& f, Args&&... args)
{
    const auto success = (((eventType == args.type) && (f(std::forward<Args>(args)), true)) || ...);
    NX_ASSERT(success);
}

} // namespace

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

QnLegacyAuditRecord::operator AuditRecord() const
{
    return visitAllConversions(
        [this](auto&&... items)
        {
            AuditRecord record{{{authSession}}};
            record.eventType = static_cast<AuditRecordType>(eventType);
            record.createdTimeS = std::chrono::seconds(createdTimeSec);
            switchByRecordType(record.eventType,
                [this, &record](auto&& conversion)
                {
                    conversion.verifyParamsToDetails(conversion.paramsToDetails, *this, record);
                },
                items...);
            return record;
        });
}

QnLegacyAuditRecord::QnLegacyAuditRecord(nx::vms::api::AuditRecord auditRecord):
    authSession(std::move(auditRecord.authSession))
{
    visitAllConversions(
        [this, &auditRecord](auto&&... items)
        {
            eventType = static_cast<Qn::LegacyAuditRecordType>(auditRecord.eventType);
            createdTimeSec = auditRecord.createdTimeS.count();
            switchByRecordType(
                auditRecord.eventType,
                [this, &auditRecord](auto&& conversion)
                {
                    conversion.verifyDetailsToParams(conversion.detailsToParams, auditRecord, *this);
                },
                items...);
        });
}

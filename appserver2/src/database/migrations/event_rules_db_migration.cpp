#include "event_rules_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/db/sql_query_execution_helper.h>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/rule.h>

using namespace nx;

namespace ec2 {
namespace db {

// Enumerator values from v2.2.
namespace eventV22 {

enum BusinessEventType
{
    NotDefinedEvent,
    Camera_Motion,
    Camera_Input,
    Camera_Disconnect,
    Storage_Failure,
    Network_Issue,
    Camera_Ip_Conflict,
    MediaServer_Failure,
    MediaServer_Conflict,
    MediaServer_Started
};

enum BusinessActionType
{
    undefinedAction,
    CameraOutput,
    Bookmark,
    CameraRecording,
    PanicRecording,
    SendMail,
    Diagnostics,
    ShowPopup,
    CameraOutputInstant,
    PlaySound,
    SayText,
    PlaySoundRepeated
};

} // namespace eventV22

// Enumerator values from v2.3.
namespace eventV23 {

enum BusinessActionType
{
    CameraOutputOnceAction = 2
};

} // namespace eventV23

int EventTypesMap[][2] =
{
    { eventV22::Camera_Motion,        nx::vms::event::cameraMotionEvent     },
    { eventV22::Camera_Input,         nx::vms::event::cameraInputEvent      },
    { eventV22::Camera_Disconnect,    nx::vms::event::cameraDisconnectEvent },
    { eventV22::Storage_Failure,      nx::vms::event::storageFailureEvent   },
    { eventV22::Network_Issue,        nx::vms::event::networkIssueEvent     },
    { eventV22::Camera_Ip_Conflict,   nx::vms::event::cameraIpConflictEvent },
    { eventV22::MediaServer_Failure,  nx::vms::event::serverFailureEvent    },
    { eventV22::MediaServer_Conflict, nx::vms::event::serverConflictEvent   },
    { eventV22::MediaServer_Started,  nx::vms::event::serverStartEvent      },
    { -1,                                  -1                               }
};

int ActionTypesMap[][2] =
{
    { eventV22::CameraOutput,        nx::vms::event::cameraOutputAction     },
    { eventV22::Bookmark,            nx::vms::event::bookmarkAction         },
    { eventV22::CameraRecording,     nx::vms::event::cameraRecordingAction  },
    { eventV22::PanicRecording,      nx::vms::event::panicRecordingAction   },
    { eventV22::SendMail,            nx::vms::event::sendMailAction         },
    { eventV22::Diagnostics,         nx::vms::event::diagnosticsAction      },
    { eventV22::ShowPopup,           nx::vms::event::showPopupAction        },
    { eventV22::CameraOutputInstant, eventV23::CameraOutputOnceAction       },
    { eventV22::PlaySound,           nx::vms::event::playSoundOnceAction    },
    { eventV22::SayText,             nx::vms::event::sayTextAction          },
    { eventV22::PlaySoundRepeated,   nx::vms::event::playSoundAction        },
    { -1,                                 -1                                }
};

int remapValue(int oldVal, const int remapData[][2])
{
    for (int i = 0; remapData[i][0] >= 0; ++i)
    {
        if (remapData[i][0] == oldVal)
            return remapData[i][1];
    }
    return oldVal;
}

struct EventRuleRemapData
{
    EventRuleRemapData() : id(0), eventType(0), actionType(0) {}

    int id;
    int eventType;
    int actionType;
    QByteArray actionParams;
};

struct CameraOutputParametersV23
{
    QString relayOutputId;
    int relayAutoResetTimeout;
};
#define CameraOutputParametersV23_Fields (relayOutputId)(relayAutoResetTimeout)

struct CameraOutputParametersV30
{
    QString relayOutputId;
    int durationMs;
};
#define CameraOutputParametersV30_Fields (relayOutputId)(durationMs)

namespace eventV30 {

enum UserGroup
{
    EveryOne = 0,
    AdminOnly = 1,
};
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(UserGroup)

} // namespace eventV30

struct ShowPopupParametersV30
{
    eventV30::UserGroup userGroup;
};
#define ShowPopupParametersV30_Fields (userGroup)

struct ShowPopupParametersV31Alpha
{
    std::vector<QnUuid> additionalResources;
};
#define ShowPopupParametersV31Alpha_Fields (additionalResources)

struct ActionParameters31Beta
{
    bool needConfirmation = false;
    QnUuid actionResourceId;
    QString url;
    QString emailAddress;
    int fps = 10;
    Qn::StreamQuality streamQuality = Qn::QualityHighest;
    int recordAfter = 0;
    QString relayOutputId;
    QString sayText;
    QString tags;
    QString text;
    int durationMs = 5000;
    std::vector<QnUuid> additionalResources;
    bool allUsers = false;
    bool forced = true;
    QString presetId;
    bool useSource = false;
    int recordBeforeMs = 1000;
    bool playToClient = true;
    QString contentType;
};
#define ActionParameters31Beta_Fields (needConfirmation)(actionResourceId)\
    (url)(emailAddress)(fps)(streamQuality)(recordAfter)(relayOutputId)(sayText)(tags)(text)\
    (durationMs)(additionalResources)(allUsers)(forced)(presetId)(useSource)(recordBeforeMs)\
    (playToClient)(contentType)

struct EventMetaData31Beta
{
    std::vector<QnUuid> cameraRefs;
    std::vector<QnUuid> instigators;
    bool allUsers = false;
};
#define EventMetaData31Beta_Fields (cameraRefs)(instigators)(allUsers)

struct EventParameters31Beta
{
    vms::event::EventType eventType;
    qint64 eventTimestampUsec;
    QnUuid eventResourceId;
    QString resourceName;
    QnUuid sourceServerId;
    vms::event::EventReason reasonCode;
    QString inputPortId;
    QString caption;
    QString description;
    EventMetaData31Beta metadata;
};
#define EventParameters31Beta_Fields \
    (eventType)(eventTimestampUsec)(eventResourceId)(resourceName)(sourceServerId) \
    (reasonCode)(inputPortId)(caption)(description)(metadata)

#define MIGRATION_ACTION_PARAM_TYPES \
    (CameraOutputParametersV23)\
    (CameraOutputParametersV30)\
    (ShowPopupParametersV30)\
    (ShowPopupParametersV31Alpha)\
    (ActionParameters31Beta)\
    (EventMetaData31Beta)\
    (EventParameters31Beta)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(MIGRATION_ACTION_PARAM_TYPES, (json))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    MIGRATION_ACTION_PARAM_TYPES, (json), _Fields, (brief, true))

bool doRemap(const QSqlDatabase& database, int id, const QVariant& newVal, const QString& fieldName)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = QString("UPDATE vms_businessrule set %1 = ? where id = ?").arg(fieldName);
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    query.addBindValue(newVal);
    query.addBindValue(id);
    return nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool migrateRulesToV23(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = "SELECT id,event_type, action_type from vms_businessrule";
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<EventRuleRemapData> oldData;
    while (query.next())
    {
        EventRuleRemapData data;
        data.id = query.value("id").toInt();
        data.eventType = query.value("event_type").toInt();
        data.actionType = query.value("action_type").toInt();
        oldData << data;
    }

    for (const EventRuleRemapData& remapData : oldData)
    {
        if (!doRemap(database, remapData.id, remapValue(remapData.eventType, EventTypesMap), "event_type"))
            return false;
        if (!doRemap(database, remapData.id, remapValue(remapData.actionType, ActionTypesMap), "action_type"))
            return false;
    }

    return true;
}

bool migrateRulesToV30(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"sql(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ? or action_type = ?
    )sql";
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    /* Updating duration field. */
    query.addBindValue(eventV23::CameraOutputOnceAction);
    query.addBindValue(nx::vms::event::cameraOutputAction);
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<EventRuleRemapData> oldData;
    while (query.next())
    {
        EventRuleRemapData data;
        data.id = query.value("id").toInt();
        data.actionType = query.value("action_type").toInt();
        data.actionParams = query.value("action_params").toByteArray();
        oldData << data;
    }

    for (const EventRuleRemapData& data: oldData)
    {
        auto oldParams = QJson::deserialized<CameraOutputParametersV23>(data.actionParams);
        CameraOutputParametersV30 newParams;
        newParams.relayOutputId = oldParams.relayOutputId;
        newParams.durationMs = oldParams.relayAutoResetTimeout;

        if (data.actionType == eventV23::CameraOutputOnceAction)
        {
            if (!doRemap(database, data.id, nx::vms::event::cameraOutputAction, "action_type"))
                return false;
        }

        if (!doRemap(database, data.id, QJson::serialized(newParams), "action_params"))
            return false;
    }

    return true;
}

bool migrateRulesToV31Alpha(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"sql(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ?
    )sql";
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    query.addBindValue(vms::event::showPopupAction);
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<EventRuleRemapData> oldData;
    while (query.next())
    {
        EventRuleRemapData data;
        data.id = query.value("id").toInt();
        data.actionParams = query.value("action_params").toByteArray();
        oldData << data;
    }

    for (const EventRuleRemapData& data: oldData)
    {
        auto oldParams = QJson::deserialized<ShowPopupParametersV30>(data.actionParams);
        ShowPopupParametersV31Alpha newParams;
        if (oldParams.userGroup == eventV30::AdminOnly)
        {
            newParams.additionalResources =
                QnUserRolesManager::adminRoleIds().toVector().toStdVector();
        }

        if (!doRemap(database, data.id, QJson::serialized(newParams), "action_params"))
            return false;
    }

    return true;
}

bool migrateActionsAllUsers(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"sql(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ? or action_type = ? or action_type = ?
           or action_type = ? or action_type = ? or action_type = ?
    )sql";
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    query.addBindValue(vms::event::showPopupAction);
    query.addBindValue(vms::event::showOnAlarmLayoutAction);
    query.addBindValue(vms::event::bookmarkAction);
    query.addBindValue(vms::event::playSoundAction);
    query.addBindValue(vms::event::playSoundOnceAction);
    query.addBindValue(vms::event::sayTextAction);
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<EventRuleRemapData> oldData;
    while (query.next())
    {
        EventRuleRemapData data;
        data.id = query.value("id").toInt();
        data.actionParams = query.value("action_params").toByteArray();
        oldData << data;
    }

    for (const EventRuleRemapData& data: oldData)
    {
        auto params = QJson::deserialized<ActionParameters31Beta>(data.actionParams);
        const bool allUsers = params.additionalResources.empty();

        if (params.allUsers == allUsers)
            continue;

        params.allUsers = allUsers;
        if (!doRemap(database, data.id, QJson::serialized(params), "action_params"))
            return false;
    }

    return true;
}

bool migrateEventsAllUsers(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"sql(
        SELECT id, event_condition
        FROM vms_businessrule
        WHERE event_type = ?
    )sql";
    if (!nx::utils::db::SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    query.addBindValue(vms::event::softwareTriggerEvent);
    if (!nx::utils::db::SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<QPair<int, QByteArray>> oldData;
    while (query.next())
    {
        oldData << qMakePair(
            query.value("id").toInt(),
            query.value("event_condition").toByteArray());
    }

    for (const auto& old: oldData)
    {
        const int id = old.first;
        auto params = QJson::deserialized<EventParameters31Beta>(old.second);

        const bool allUsers = params.metadata.instigators.empty();
        if (params.metadata.allUsers == allUsers)
            continue;

        params.metadata.allUsers = allUsers;

        if (!doRemap(database, id, QJson::serialized(params), "event_condition"))
            return false;
    }

    return true;
}

} // namespace db
} // namespace ec2

QN_FUSION_DECLARE_FUNCTIONS(ec2::db::eventV30::UserGroup, (metatype)(lexical))
QN_FUSION_DEFINE_FUNCTIONS(ec2::db::eventV30::UserGroup, (numeric))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ec2::db::eventV30, UserGroup,
    (ec2::db::eventV30::EveryOne, "EveryOne")
    (ec2::db::eventV30::AdminOnly, "AdminOnly"))

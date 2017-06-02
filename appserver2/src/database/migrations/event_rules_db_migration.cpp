#include "event_rules_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <nx/vms/event/event_fwd.h>

#include <utils/db/db_helper.h>
#include <nx/fusion/model_functions.h>

namespace ec2 {
namespace db {

/* Enumerator values from v2.2. */
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
    UndefinedAction,
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

/* Enumerator values from v2.3. */
namespace eventV23 {

enum BusinessActionType
{
    CameraOutputOnceAction = 2
};

} // namespace eventV23

int EventTypesMap[][2] =
{
    { eventV22::Camera_Motion,        nx::vms::event::CameraMotionEvent     },
    { eventV22::Camera_Input,         nx::vms::event::CameraInputEvent      },
    { eventV22::Camera_Disconnect,    nx::vms::event::CameraDisconnectEvent },
    { eventV22::Storage_Failure,      nx::vms::event::StorageFailureEvent   },
    { eventV22::Network_Issue,        nx::vms::event::NetworkIssueEvent     },
    { eventV22::Camera_Ip_Conflict,   nx::vms::event::CameraIpConflictEvent },
    { eventV22::MediaServer_Failure,  nx::vms::event::ServerFailureEvent    },
    { eventV22::MediaServer_Conflict, nx::vms::event::ServerConflictEvent   },
    { eventV22::MediaServer_Started,  nx::vms::event::ServerStartEvent      },
    { -1,                                  -1                               }
};

int ActionTypesMap[][2] =
{
    { eventV22::CameraOutput,        nx::vms::event::CameraOutputAction     },
    { eventV22::Bookmark,            nx::vms::event::BookmarkAction         },
    { eventV22::CameraRecording,     nx::vms::event::CameraRecordingAction  },
    { eventV22::PanicRecording,      nx::vms::event::PanicRecordingAction   },
    { eventV22::SendMail,            nx::vms::event::SendMailAction         },
    { eventV22::Diagnostics,         nx::vms::event::DiagnosticsAction      },
    { eventV22::ShowPopup,           nx::vms::event::ShowPopupAction        },
    { eventV22::CameraOutputInstant, eventV23::CameraOutputOnceAction       },
    { eventV22::PlaySound,           nx::vms::event::PlaySoundOnceAction    },
    { eventV22::SayText,             nx::vms::event::SayTextAction          },
    { eventV22::PlaySoundRepeated,   nx::vms::event::PlaySoundAction        },
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

struct BusinessRuleRemapData
{
    BusinessRuleRemapData() : id(0), eventType(0), actionType(0) {}

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

#define MIGRATION_ACTION_PARAM_TYPES \
    (CameraOutputParametersV23)\
    (CameraOutputParametersV30)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(MIGRATION_ACTION_PARAM_TYPES, (json))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    MIGRATION_ACTION_PARAM_TYPES, (json), _Fields, (optional, false))

bool doRemap(const QSqlDatabase& database, int id, const QVariant& newVal, const QString& fieldName)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = QString("UPDATE vms_businessrule set %1 = ? where id = ?").arg(fieldName);
    if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    query.addBindValue(newVal);
    query.addBindValue(id);
    return QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool migrateBusinessRulesToV23(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = "SELECT id,event_type, action_type from vms_businessrule";
    if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<BusinessRuleRemapData> oldData;
    while (query.next())
    {
        BusinessRuleRemapData data;
        data.id = query.value("id").toInt();
        data.eventType = query.value("event_type").toInt();
        data.actionType = query.value("action_type").toInt();
        oldData << data;
    }

    for (const BusinessRuleRemapData& remapData : oldData)
    {
        if (!doRemap(database, remapData.id, remapValue(remapData.eventType, EventTypesMap), "event_type"))
            return false;
        if (!doRemap(database, remapData.id, remapValue(remapData.actionType, ActionTypesMap), "action_type"))
            return false;
    }

    return true;
}

bool migrateBusinessRulesToV30(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ? or action_type = ?
    )";
    if (!QnDbHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    /* Updating duration field. */
    query.addBindValue(eventV23::CameraOutputOnceAction);
    query.addBindValue(nx::vms::event::CameraOutputAction);
    if (!QnDbHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<BusinessRuleRemapData> oldData;
    while (query.next())
    {
        BusinessRuleRemapData data;
        data.id = query.value("id").toInt();
        data.actionType = query.value("action_type").toInt();
        data.actionParams = query.value("action_params").toByteArray();
        oldData << data;
    }

    for (const BusinessRuleRemapData& data : oldData)
    {
        auto oldParams = QJson::deserialized<CameraOutputParametersV23>(data.actionParams);
        CameraOutputParametersV30 newParams;
        newParams.relayOutputId = oldParams.relayOutputId;
        newParams.durationMs = oldParams.relayAutoResetTimeout;

        if (data.actionType == eventV23::CameraOutputOnceAction)
        {
            if (!doRemap(database, data.id, nx::vms::event::CameraOutputAction, "action_type"))
                return false;
        }

        if (!doRemap(database, data.id, QJson::serialized(newParams), "action_params"))
            return false;
    }

    return true;
}

}
}

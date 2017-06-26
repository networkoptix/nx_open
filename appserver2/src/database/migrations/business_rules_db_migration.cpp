#include "business_rules_db_migration.h"

#include <QtCore/QVariant>

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>

#include <core/resource_management/user_roles_manager.h>

#include <business/business_fwd.h>

#include <utils/db/db_helper.h>
#include <nx/fusion/model_functions.h>

namespace ec2 {
namespace db {

    /* Enumerator values from v2.2. */
namespace QnBusinessV22 {
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
}

namespace QnBusinessV23 {
enum BusinessActionType
{
    CameraOutputOnceAction = 2
};
}

int EventTypesMap[][2] =
{
    { QnBusinessV22::Camera_Motion,        QnBusiness::CameraMotionEvent     },
    { QnBusinessV22::Camera_Input,         QnBusiness::CameraInputEvent      },
    { QnBusinessV22::Camera_Disconnect,    QnBusiness::CameraDisconnectEvent },
    { QnBusinessV22::Storage_Failure,      QnBusiness::StorageFailureEvent   },
    { QnBusinessV22::Network_Issue,        QnBusiness::NetworkIssueEvent     },
    { QnBusinessV22::Camera_Ip_Conflict,   QnBusiness::CameraIpConflictEvent },
    { QnBusinessV22::MediaServer_Failure,  QnBusiness::ServerFailureEvent    },
    { QnBusinessV22::MediaServer_Conflict, QnBusiness::ServerConflictEvent   },
    { QnBusinessV22::MediaServer_Started,  QnBusiness::ServerStartEvent      },
    { -1,                                  -1                                }
};

int ActionTypesMap[][2] =
{
    { QnBusinessV22::CameraOutput,        QnBusiness::CameraOutputAction     },
    { QnBusinessV22::Bookmark,            QnBusiness::BookmarkAction         },
    { QnBusinessV22::CameraRecording,     QnBusiness::CameraRecordingAction  },
    { QnBusinessV22::PanicRecording,      QnBusiness::PanicRecordingAction   },
    { QnBusinessV22::SendMail,            QnBusiness::SendMailAction         },
    { QnBusinessV22::Diagnostics,         QnBusiness::DiagnosticsAction      },
    { QnBusinessV22::ShowPopup,           QnBusiness::ShowPopupAction        },
    { QnBusinessV22::CameraOutputInstant, QnBusinessV23::CameraOutputOnceAction },
    { QnBusinessV22::PlaySound,           QnBusiness::PlaySoundOnceAction    },
    { QnBusinessV22::SayText,             QnBusiness::SayTextAction          },
    { QnBusinessV22::PlaySoundRepeated,   QnBusiness::PlaySoundAction        },
    { -1,                                 -1                                 }
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

struct ShowPopupParametersV30
{
    QnBusiness::UserGroup userGroup;
};
#define ShowPopupParametersV30_Fields (userGroup)

struct ShowPopupParametersV31Alpha
{
    std::vector<QnUuid> additionalResources;
};
#define ShowPopupParametersV31Alpha_Fields (additionalResources)

#define MIGRATION_ACTION_PARAM_TYPES \
    (CameraOutputParametersV23)\
    (CameraOutputParametersV30)\
    (ShowPopupParametersV30)\
    (ShowPopupParametersV31Alpha)\

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(MIGRATION_ACTION_PARAM_TYPES, (json))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    MIGRATION_ACTION_PARAM_TYPES, (json), _Fields, (optional, false))

bool doRemap(const QSqlDatabase& database, int id, const QVariant& newVal, const QString& fieldName)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = QString("UPDATE vms_businessrule set %1 = ? where id = ?").arg(fieldName);
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    query.addBindValue(newVal);
    query.addBindValue(id);
    return SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO);
}

bool migrateBusinessRulesToV23(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = "SELECT id,event_type, action_type from vms_businessrule";
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;
    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
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
    QString sqlText = R"sql(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ? or action_type = ?
    )sql";
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    /* Updating duration field. */
    query.addBindValue(QnBusinessV23::CameraOutputOnceAction);
    query.addBindValue(QnBusiness::CameraOutputAction);
    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
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

        if (data.actionType == QnBusinessV23::CameraOutputOnceAction)
        {
            if (!doRemap(database, data.id, QnBusiness::CameraOutputAction, "action_type"))
                return false;
        }

        if (!doRemap(database, data.id, QJson::serialized(newParams), "action_params"))
            return false;
    }

    return true;
}

bool migrateBusinessRulesToV31Alpha(const QSqlDatabase& database)
{
    QSqlQuery query(database);
    query.setForwardOnly(true);
    QString sqlText = R"sql(
        SELECT id, action_type, action_params
        FROM vms_businessrule
        WHERE action_type = ?
    )sql";
    if (!SqlQueryExecutionHelper::prepareSQLQuery(&query, sqlText, Q_FUNC_INFO))
        return false;

    /* Updating duration field. */
    query.addBindValue(QnBusiness::ShowPopupAction);
    if (!SqlQueryExecutionHelper::execSQLQuery(&query, Q_FUNC_INFO))
        return false;

    QVector<BusinessRuleRemapData> oldData;
    while (query.next())
    {
        BusinessRuleRemapData data;
        data.id = query.value("id").toInt();
        data.actionParams = query.value("action_params").toByteArray();
        oldData << data;
    }

    for (const BusinessRuleRemapData& data: oldData)
    {
        auto oldParams = QJson::deserialized<ShowPopupParametersV30>(data.actionParams);
        ShowPopupParametersV31Alpha newParams;
        if (oldParams.userGroup == QnBusiness::AdminOnly)
        {
            newParams.additionalResources =
                QnUserRolesManager::adminRoleIds().toVector().toStdVector();
        }

        if (!doRemap(database, data.id, QJson::serialized(newParams), "action_params"))
            return false;
    }

    return true;
}

}
}

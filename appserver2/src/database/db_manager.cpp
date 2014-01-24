#include "db_manager.h"
#include <QtSql/QtSql>

namespace ec2
{

static QnDbManager* globalInstance = 0;

QnDbManager* QnDbManager::instance()
{
    return globalInstance;
}

void QnDbManager::initStaticInstance(QnDbManager* value)
{
    globalInstance = value;
}

ErrorCode QnDbManager::executeTransaction( const QnTransaction<ApiCameraData>& tran)
{
    QSqlQuery insQuery(*m_sdb);
    insQuery.prepare("INSERT INTO vms_resource (id, guid, xtype_id, parent_id, name, url, status, disabled) VALUES(:id, :guid, :typeId, :parentId, :name, :url, :status, :disabled)");

    static_cast<ApiResourceData>(tran.params).autoBindValues(insQuery);
    //delQuery.bindValue(":id1", resId.toInt());

    return ErrorCode::ok;
}

}

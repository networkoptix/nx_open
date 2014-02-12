#ifndef __RESOURCE_TRANSACTION_DATA_H__
#define __RESOURCE_TRANSACTION_DATA_H__

#include "api_data.h"
#include "core/resource/resource_fwd.h"
#include "core/resource/resource.h"

#include <QtCore/qglobal.h>
#include <QString>
#include <vector>


namespace ec2
{

struct ApiResourceParam: public ApiData
{
    ApiResourceParam() {}
    ApiResourceParam(qint32 resourceId, const QString& name, const QString& value): resourceId(resourceId), name(name), value(value) {}

    qint32 resourceId;  //TODO this value MUST be the same for every item in the list, so it's not appropriate here
    QString name;
    QString value;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

typedef std::vector<ApiResourceParam> ApiResourceParams;

struct ApiResourceData: public ApiData 
{
    ApiResourceData(): id(0), typeId(0), parentId(0), status(QnResource::Offline), disabled(false) {}

    qint32        id;
    QString       guid;
    qint32        typeId;
    qint32        parentId;
    QString       name;
    QString       url;
    QnResource::Status    status;
    bool          disabled;
    std::vector<ApiResourceParam> addParams;

	void fromResource(const QnResourcePtr& resource);
	void toResource(QnResourcePtr resource) const;
	QnResourceParameters toHashMap() const;
    QN_DECLARE_STRUCT_SQL_BINDER();
};

struct ApiResourceDataList: public ApiData {
	void loadFromQuery(QSqlQuery& query);
	void toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const;

	std::vector<ApiResourceData> data;
};

struct ApiSetResourceStatusData: public ApiData
{
    qint32 id;
    QnResource::Status    status;

    QN_DECLARE_STRUCT_SQL_BINDER();
};

}

#define ApiResourceParamFields (resourceId) (name) (value)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceParam, ApiResourceParamFields)

#define ApiResourceDataFields (id) (guid) (typeId) (parentId) (name) (url) (status) (disabled) (addParams)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceData,  ApiResourceDataFields)
QN_DEFINE_STRUCT_BINARY_SERIALIZATION_FUNCTIONS (ec2::ApiResourceDataList,  (data))
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiSetResourceStatusData,  (id) (status) )

#endif // __RESOURCE_TRANSACTION_DATA_H__

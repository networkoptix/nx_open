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

	void fromResource(const QnResourcePtr& resource);
	void toResource(QnResourcePtr resource) const;
    QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
};

}

#define ApiResourceDataFields (id) (guid) (typeId) (parentId) (name) (url) (status) (disabled)
QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceData,  ApiResourceDataFields)

#endif // __RESOURCE_TRANSACTION_DATA_H__

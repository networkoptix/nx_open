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

struct ApiResourceData: public ApiData {
    qint32        id;
    QString       guid;
    qint32        typeId;
    qint32        parentId;
    QString       name;
    QString       url;
    QnResource::Status    status;
    bool          disabled;

	void fromResource(const QnResourcePtr& resource);
	void toResource(QnResourcePtr resource);
    QN_DECLARE_STRUCT_SERIALIZATORS_BINDERS();
};

}

QN_DEFINE_STRUCT_SERIALIZATORS_BINDERS (ec2::ApiResourceData, (id) (guid) (typeId) (parentId) (name) (url) (status) (disabled) )

#endif // __RESOURCE_TRANSACTION_DATA_H__

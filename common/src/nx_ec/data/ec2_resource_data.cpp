#include "ec2_resource_data.h"
#include "core/resource/resource.h"

namespace ec2 {

void ApiResourceData::fromResource(const QnResourcePtr& resource)
{
	id = resource->getId();
	guid = resource->getGuid();
	typeId = resource->getTypeId();
	parentId = resource->getParentId();
	name = resource->getName();
	url = resource->getUrl();
	status = resource->getStatus();
	disabled = resource->isDisabled();

    QnParamList params = resource->getResourceParamList();
    foreach(const QnParam& param, resource->getResourceParamList().list())
    {
        if (param.domain() == QnDomainDatabase)
            addParams.push_back(ApiResourceParam(id, param.name(), param.value().toString()));
    }
}

void ApiResourceData::toResource(QnResourcePtr resource) const
{
	resource->setId(id);
	resource->setGuid(guid);
	resource->setTypeId(typeId);
	resource->setParentId(parentId);
	resource->setName(name);
	resource->setUrl(url);
	resource->setStatus(status, true);
	resource->setDisabled(disabled);

    foreach(const ApiResourceParam& param, addParams)
        resource->setParam(param.name, param.value, QnDomainDatabase);
}

QnResourceParameters ApiResourceData::toHashMap() const
{
	QnResourceParameters parameters;
	parameters["id"] = QString::number(id);
	parameters["name"] = name;
	parameters["url"] = url;
	parameters["status"] = QString::number(status);
	parameters["disabled"] = QString::number(disabled);
	parameters["parentId"] = QString::number(parentId);

	return parameters;
}


void ApiResourceDataList::loadFromQuery(QSqlQuery& query)
{
    //TODO/IMPL
    assert( false );
}

void ApiResourceDataList::toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const
{
	resList.reserve(data.size());
	for(int i = 0; i < data.size(); ++i) {
        QnResourcePtr res = resFactory->createResource(
            data[i].typeId,
            data[i].toHashMap() ).dynamicCast<QnResource>();
		data[i].toResource( res );
		resList << res;
	}
}

}

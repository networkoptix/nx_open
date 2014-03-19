#include "ec2_resource_data.h"
#include "core/resource/resource.h"

namespace ec2 {

void ApiResourceData::fromResource(const QnResourcePtr& resource)
{
    Q_ASSERT(!resource->getId().isNull());
    Q_ASSERT(!resource->getTypeId().isNull());

	id = resource->getId();
	typeId = resource->getTypeId();
	parentGuid = resource->getParentId();
	name = resource->getName();
	url = resource->getUrl();
	status = resource->getStatus();
	disabled = resource->isDisabled();

    QnParamList params = resource->getResourceParamList();
    foreach(const QnParam& param, resource->getResourceParamList().list())
    {
        if (param.domain() == QnDomainDatabase)
            addParams.push_back(ApiResourceParam(param.name(), param.value().toString(), true));
    }
}

void ApiResourceData::toResource(QnResourcePtr resource) const
{
	resource->setId(id);
	//resource->setGuid(guid);
	resource->setTypeId(typeId);
	resource->setParentId(parentGuid);
	resource->setName(name);
	resource->setUrl(url);
	resource->setStatus(status, true);
	resource->setDisabled(disabled);

    foreach(const ApiResourceParam& param, addParams) {
        if (param.isResTypeParam)
            resource->setParam(param.name, param.value, QnDomainDatabase);
    }
}

void ApiResourceDataList::loadFromQuery(QSqlQuery& /*query*/)
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
            QnResourceParams(data[i].url, QString() )).dynamicCast<QnResource>();
		data[i].toResource( res );
		resList << res;
	}
}

void ApiParamList::fromResourceList(const QnKvPairList& resources)
{
    data.resize(resources.size());
    for (int i = 0; i < resources.size(); ++i)
    {
        data[i].name = resources[i].name();
        data[i].value = resources[i].value();
        data[i].isResTypeParam = false;
    }
}

void ApiParamList::toResourceList(QnKvPairList& resources) const
{
    resources.reserve(data.size());
    foreach(const ApiResourceParam& param, data)
    {
        resources << QnKvPair();
        resources.last().setName(param.name);
        resources.last().setValue(param.value);
    }
}


}

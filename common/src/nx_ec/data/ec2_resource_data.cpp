#include "ec2_resource_data.h"
#include "core/resource/resource.h"



namespace ec2 {

    //QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceParamData, ApiResourceParamFields);
    QN_FUSION_DECLARE_FUNCTIONS(ApiResourceParamData, (binary))

        //QN_DEFINE_STRUCT_SERIALIZATORS(ApiResourceData,  ApiResourceFields)
        QN_FUSION_DECLARE_FUNCTIONS(ApiResourceData, (binary))


void fromResourceToApi(const QnResourcePtr& resource, ApiResourceData& data)
{
    Q_ASSERT(!resource->getId().isNull());
    Q_ASSERT(!resource->getTypeId().isNull());

	data.id = resource->getId();
	data.typeId = resource->getTypeId();
	data.parentGuid = resource->getParentId();
	data.name = resource->getName();
	data.url = resource->getUrl();
	data.status = resource->getStatus();

    QnParamList params = resource->getResourceParamList();
    foreach(const QnParam& param, resource->getResourceParamList().list())
    {
        if (param.domain() == QnDomainDatabase)
            data.addParams.push_back(ApiResourceParamData(param.name(), param.value().toString(), true));
    }
}

void fromApiToResource(const ApiResourceData& data, QnResourcePtr resource)
{
	resource->setId(data.id);
	//resource->setGuid(guid);
	resource->setTypeId(data.typeId);
	resource->setParentId(data.parentGuid);
	resource->setName(data.name);
	resource->setUrl(data.url);
	resource->setStatus(data.status, true);

    foreach(const ApiResourceParamData& param, data.addParams) {
        if (param.isResTypeParam)
            resource->setParam(param.name, param.value, QnDomainDatabase);
        else
            resource->setProperty(param.name, param.value);
    }
}

void ApiResourceList::loadFromQuery(QSqlQuery& /*query*/)
{
    //TODO/IMPL
    assert( false );
}

void ApiResourceList::toResourceList( QnResourceFactory* resFactory, QnResourceList& resList ) const
{
	resList.reserve(data.size());
	for(int i = 0; i < data.size(); ++i) {
        QnResourcePtr res = resFactory->createResource(
            data[i].typeId,
            QnResourceParams(data[i].url, QString() )).dynamicCast<QnResource>();
		fromApiToResource(data[i],  res);
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
    foreach(const ApiResourceParamData& param, data)
    {
        resources << QnKvPair();
        resources.last().setName(param.name);
        resources.last().setValue(param.value);
    }
}


}

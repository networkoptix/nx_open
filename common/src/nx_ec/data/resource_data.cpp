#include "resource_data.h"
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
}

void ApiResourceData::toResource(QnResourcePtr resource)
{
	resource->setId(id);
	resource->setGuid(guid);
	resource->setTypeId(typeId);
	resource->setParentId(parentId);
	resource->setName(name);
	resource->setUrl(url);
	resource->setStatus(status);
	resource->setDisabled(disabled);
}

}

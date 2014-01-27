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
	status = (ec2::Status) resource->getStatus();
	disabled = resource->isDisabled();
}

}

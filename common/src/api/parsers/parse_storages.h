#ifndef _eve_parse_storages_
#define _eve_parse_storages_

#include "api/Types.h"
#include "core/resource/resource.h"
#include "core/resource/qnstorage.h"

template <class ResourcePtr>
void parseStorages(QList<ResourcePtr>& storages, const QnApiStorages& xsdStorages, QnResourceFactory& resourceFactory)
{
    using xsd::api::storages::Storages;
    using xsd::api::resourceTypes::ParentIDs;
    using xsd::api::resourceTypes::PropertyTypes;
    using xsd::api::resourceTypes::ParamType;

    for (Storages::storage_const_iterator i (xsdStorages.begin()); i != xsdStorages.end(); ++i)
    {
        QnStoragePtr storage(new QnStorage());
        storage->setId(i->id().c_str());
        storage->setName(i->name().c_str());
        storage->setTypeId(i->typeId().c_str());
        if (i->parentId().present())
            storage->setParentId((*i->parentId()).c_str());

        storage->setUrl(i->url().c_str());

        storage->setSpaceLimit(i->spaceLimit());
        storage->setMaxStoreTime(i->time());
        storage->setIndex(i->index());

        storages.append(storage);
    }
}

#endif // _eve_parse_storages_

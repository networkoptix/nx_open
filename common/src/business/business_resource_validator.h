#ifndef BUSINESS_RESOURCE_VALIDATOR_H
#define BUSINESS_RESOURCE_VALIDATOR_H

#include <core/resource/resource_fwd.h>
#include <core/resource/shared_resource_pointer.h>

template<typename CheckingPolicy>
class QnBusinessResourceValidator: public CheckingPolicy {
    using CheckingPolicy::emptyListIsValid;
    using CheckingPolicy::isResourceValid;
    using CheckingPolicy::getErrorText;

    typedef typename CheckingPolicy::resource_type ResourceType;
public:
    static int invalidResourcesCount(const QnResourceList &resources) {
        QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
        int invalid = 0;
        foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
            if (!isResourceValid(resource))
                invalid++;
        return invalid;
    }

    static bool isResourcesListValid(const QnResourceList &resources) {
        QnSharedResourcePointerList<ResourceType> filtered = resources.filtered<ResourceType>();
        if (filtered.isEmpty())
            return emptyListIsValid();
        foreach (const QnSharedResourcePointer<ResourceType> &resource, filtered)
            if (!isResourceValid(resource))
                return false;
        return true;
    }

};

#endif // BUSINESS_RESOURCE_VALIDATOR_H

#ifndef QN_SHARED_RESOURCE_POINTER_LIST_H
#define QN_SHARED_RESOURCE_POINTER_LIST_H

#include <iterator>

#include <QtCore/QList>
#include <QtCore/QtAlgorithms>

#include <core/resourcemanagment/resource_criterion.h>

#include "shared_resource_pointer.h"

template<class Resource>
class QnSharedResourcePointerList: public QList<QnSharedResourcePointer<Resource> > {
    typedef QList<QnSharedResourcePointer<Resource> > base_type;

public:
    QnSharedResourcePointerList() {}

    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<Resource> > &other): base_type(other) {}

    template<class Y>
    explicit QnSharedResourcePointerList(const QList<Y> &other) {
        reserve(other.size());
        qCopy(other.begin(), other.end(), std::back_inserter(*this));
    }

    QnSharedResourcePointerList<T> filter(const QnResourceCriterion &criterion) {

    }

    template<class Resource, class Result>
    Result filter() {
        Result result;
        foreach(const QnResourcePtr &resource, resources)
            if(QnSharedResourcePointer<Resource> derived = resource.dynamicCast<Resource>())
                result.push_back(derived);
        return result;
    }

    template<class Resource>
    QList<QnSharedResourcePointer<Resource> > filter(const QnResourceList &resources) {
        return filter<Resource, QList<QnSharedResourcePointer<Resource> > >(resources);
    }


};

#endif // QN_SHARED_RESOURCE_POINTER_LIST_H

#ifndef QN_SHARED_RESOURCE_POINTER_LIST_H
#define QN_SHARED_RESOURCE_POINTER_LIST_H

#include <iterator>

#include <QtCore/QList>
#include <QtCore/QtAlgorithms>

#include "shared_resource_pointer.h"

class QnResourceCriterion;

template<class Resource>
class QnSharedResourcePointerList: public QList<QnSharedResourcePointer<Resource> > {
    typedef QList<QnSharedResourcePointer<Resource> > base_type;

public:
    QnSharedResourcePointerList() {}

    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<Resource> > &other): base_type(other) {}

    template<class OtherResource>
    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<OtherResource> > &other) {
        this->reserve(other.size());
        qCopy(other.begin(), other.end(), std::back_inserter(*this));
    }

    template<class OtherResource>
    bool operator==(const QList<QnSharedResourcePointer<OtherResource> > &other) const {
        return this->size() == other.size() && qEqual(this->begin(), this->end(), other.begin());
    }

    template<class OtherResource>
    bool operator!=(const QList<QnSharedResourcePointer<OtherResource> > &other) const {
        return !(*this == other);
    }


    /** 
     * This function is defined in <tt>resource_criterion.h</tt>.
     * 
     * \param criterion                 Resource criterion to use for filtering.
     * \returns                         Filtered list of resources.
     */
    QnSharedResourcePointerList<Resource> filtered(const QnResourceCriterion &criterion) const;

    template<class OtherResource>
    QnSharedResourcePointerList<OtherResource> filtered() const {
        QnSharedResourcePointerList<OtherResource> result;
        for(const QnSharedResourcePointer<Resource> &resource: *this)
            if(QnSharedResourcePointer<OtherResource> derived = resource.template dynamicCast<OtherResource>())
                result.push_back(derived);
        return result;
    }
};

#endif // QN_SHARED_RESOURCE_POINTER_LIST_H

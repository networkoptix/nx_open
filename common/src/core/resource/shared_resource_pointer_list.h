#pragma once

#include <iterator>
#include <initializer_list>

#include <QtCore/QList>
#include <QtCore/QtAlgorithms>

#include "shared_resource_pointer.h"
#include <functional>

template<class Resource>
class QnSharedResourcePointerList: public QList<QnSharedResourcePointer<Resource> > {
    typedef QList<QnSharedResourcePointer<Resource> > base_type;

public:
    QnSharedResourcePointerList() {}

    QnSharedResourcePointerList(std::initializer_list<QnSharedResourcePointer<Resource>> other):
        base_type(other)
    {
    }

    template<class OtherResource>
    QnSharedResourcePointerList(std::initializer_list<QnSharedResourcePointer<OtherResource>> other)
    {
        this->reserve(other.size());
        std::copy(other.begin(), other.end(), std::back_inserter(*this));
    }

    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<Resource> > &other): base_type(other) {}

    template<class OtherResource>
    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<OtherResource> > &other) {
        this->reserve(other.size());
        std::copy(other.begin(), other.end(), std::back_inserter(*this));
    }

    template<class OtherResource>
    bool operator==(const QList<QnSharedResourcePointer<OtherResource> > &other) const {
        return this->size() == other.size() && std::equal(this->begin(), this->end(), other.begin());
    }

    template<class OtherResource>
    bool operator!=(const QList<QnSharedResourcePointer<OtherResource> > &other) const {
        return !(*this == other);
    }

    template<class OtherResource>
    QnSharedResourcePointerList<OtherResource> filtered() const {
        QnSharedResourcePointerList<OtherResource> result;
        for(const QnSharedResourcePointer<Resource> &resource: *this)
            if(QnSharedResourcePointer<OtherResource> derived = resource.template dynamicCast<OtherResource>())
                result.push_back(derived);
        return result;
    }

    QnSharedResourcePointerList<Resource> filtered(std::function<bool(const QnSharedResourcePointer<Resource>&)> filter) const {
        QnSharedResourcePointerList<Resource> result;
        for(const QnSharedResourcePointer<Resource> &resource: *this)
            if(filter(resource))
                result.push_back(resource);
        return result;
    }

    template<class OtherResource>
    QnSharedResourcePointerList<OtherResource> filtered(std::function<bool(const QnSharedResourcePointer<OtherResource>&)> filter) const {
        QnSharedResourcePointerList<OtherResource> result;
        for(const QnSharedResourcePointer<Resource> &resource: *this)
            if(QnSharedResourcePointer<OtherResource> derived = resource.template dynamicCast<OtherResource>())
                if (filter(derived))
                    result.push_back(derived);
        return result;
    }
};

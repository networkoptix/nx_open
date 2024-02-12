// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <initializer_list>
#include <iterator>

#include <QtCore/QList>

#include <nx/utils/uuid.h>

#include "shared_resource_pointer.h"

template<class Resource>
class QnSharedResourcePointerList: public QList<QnSharedResourcePointer<Resource>>
{
    using base_type = QList<QnSharedResourcePointer<Resource>>;

public:
    QnSharedResourcePointerList()
    {
    }

    QnSharedResourcePointerList(std::initializer_list<QnSharedResourcePointer<Resource>> other):
        base_type(other)
    {
    }

    template <typename InputIterator, QtPrivate::IfIsInputIterator<InputIterator> = true>
    QnSharedResourcePointerList(InputIterator first, InputIterator last)
    {
        if constexpr (std::is_convertible_v<
            typename std::iterator_traits<InputIterator>::iterator_category,
            std::random_access_iterator_tag>)
        {
            this->reserve((int) (last - first));
        }

        std::copy(first, last, std::back_inserter(*this));
    }

    template<class OtherResource>
    QnSharedResourcePointerList(
        std::initializer_list<QnSharedResourcePointer<OtherResource>> other)
    {
        this->reserve((int) other.size());
        std::copy(other.begin(), other.end(), std::back_inserter(*this));
    }

    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<Resource>>& other):
        base_type(other)
    {
    }

    template<class OtherResource>
    QnSharedResourcePointerList(const QList<QnSharedResourcePointer<OtherResource>>& other)
    {
        this->reserve(other.size());
        std::copy(other.begin(), other.end(), std::back_inserter(*this));
    }

    template<class OtherResource>
    bool operator==(const QList<QnSharedResourcePointer<OtherResource>>& other) const
    {
        return this->size() == other.size()
            && std::equal(this->begin(), this->end(), other.begin());
    }

    template<class OtherResource>
    bool operator==(const QnSharedResourcePointerList<OtherResource>& other) const
    {
        return this->size() == other.size()
            && std::equal(this->begin(), this->end(), other.begin());
    }

    template<class OtherResource>
    bool operator!=(const QList<QnSharedResourcePointer<OtherResource>>& other) const
    {
        return !(*this == other);
    }

    template<class OtherResource>
    bool operator!=(const QnSharedResourcePointerList<OtherResource>& other) const
    {
        return !(*this == other);
    }

    template<class OtherResource>
    QnSharedResourcePointerList<OtherResource> filtered() const
    {
        QnSharedResourcePointerList<OtherResource> result;
        for (const auto& resource: *this)
        {
            if (const auto derived = resource.template dynamicCast<OtherResource>())
                result.push_back(derived);
        }
        return result;
    }

    template<class Filter>
    QnSharedResourcePointerList<Resource> filtered(Filter filter) const
    {
        QnSharedResourcePointerList<Resource> result;
        for (const auto& resource: *this)
        {
            if (filter(resource))
                result.push_back(resource);
        }
        return result;
    }

    template<class OtherResource, class Filter>
    QnSharedResourcePointerList<OtherResource> filtered(Filter filter) const
    {
        QnSharedResourcePointerList<OtherResource> result;
        for (const auto& resource: *this)
        {
            if (const auto derived = resource.template dynamicCast<OtherResource>())
            {
                if (filter(derived))
                    result.push_back(derived);
            }
        }
        return result;
    }

    QList<nx::Uuid> ids() const
    {
        QList<nx::Uuid> result;
        for (const auto& resource: *this)
            result.push_back(resource->getId());
        return result;
    }
};

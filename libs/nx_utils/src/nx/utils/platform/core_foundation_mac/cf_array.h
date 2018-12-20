#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include <nx/utils/platform/core_foundation_mac/cf_ref_holder.h>

namespace cf {

template<typename CFArrayRefType>
class QnCFBaseArray : public QnCFRefHolder<CFArrayRefType>
{
    typedef QnCFRefHolder<CFArrayRefType> base_type;

public:
    QnCFBaseArray();

    QnCFBaseArray(CFArrayRefType ref);

    ~QnCFBaseArray();

    int size() const;

    bool empty() const;

    template<typename ReturnType>
    ReturnType at(int index) const;

    static QnCFBaseArray makeOwned(CFArrayRefType arrayRef);
};

template<typename CFArrayRefType>
QnCFBaseArray<CFArrayRefType>::QnCFBaseArray() :
    base_type()
{}

template<typename CFArrayRefType>
QnCFBaseArray<CFArrayRefType>::QnCFBaseArray(CFArrayRefType ref) :
    base_type(ref)
{}

template<typename CFArrayRefType>
QnCFBaseArray<CFArrayRefType>::~QnCFBaseArray()
{}

template<typename CFArrayRefType>
int QnCFBaseArray<CFArrayRefType>::size() const
{
    return (base_type::ref() ? CFArrayGetCount(base_type::ref()) : 0);
}

template<typename CFArrayRefType>
bool QnCFBaseArray<CFArrayRefType>::empty() const
{
    return (!base_type::ref() || (size() <= 0));
}

template<typename CFArrayRefType>
template<typename ReturnType>
ReturnType QnCFBaseArray<CFArrayRefType>::at(int index) const
{
    return (!base_type::ref() || index < 0 || index >= size()
        ? nullptr
        : reinterpret_cast<ReturnType>(CFArrayGetValueAtIndex(base_type::ref(), index)));
}

template<typename CFArrayRefType>
QnCFBaseArray<CFArrayRefType>
    QnCFBaseArray<CFArrayRefType>::makeOwned(CFArrayRefType arrayRef)
{
    QnCFBaseArray result;
    result.setRef(arrayRef);
    return result;
}

typedef QnCFBaseArray<CFArrayRef> QnCFArray;
}

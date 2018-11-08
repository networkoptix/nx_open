#pragma once

#include <type_traits>

#include <CoreFoundation/CoreFoundation.h>

namespace cf {

template<typename CFRefType>
class QnCFRefHolder
{
public:
    // Constant only pointer to underline type
    typedef const typename std::remove_pointer<CFRefType>::type* const_cf_ref;

    explicit QnCFRefHolder(CFRefType ref);

    template<typename Target, typename Source>
    static Target makeOwned(Source ref);

    ~QnCFRefHolder();

    CFRefType ref();

    const CFRefType ref() const;

protected:
    QnCFRefHolder();

    QnCFRefHolder(const QnCFRefHolder& other);

    QnCFRefHolder& operator=(const QnCFRefHolder& other);

    void reset(CFRefType ref = nullptr);

private:
    CFRefType m_ref;
};

template<typename CFRefType>
QnCFRefHolder<CFRefType>::QnCFRefHolder()
    :
    m_ref(nullptr)
{
}

template<typename CFRefType>
QnCFRefHolder<CFRefType>::QnCFRefHolder(const QnCFRefHolder& other)
    :
    m_ref(other.ref())
{
    if (m_ref)
        CFRetain(m_ref);
}

template<typename CFRefType>
QnCFRefHolder<CFRefType>::QnCFRefHolder(CFRefType ref)
    :
    m_ref(ref)
{
}


template<typename CFRefType>
template<typename Target, typename Source>
Target QnCFRefHolder<CFRefType>::makeOwned(Source ref)
{
    return Target(reinterpret_cast<Source>(CFRetain(ref)));
}

template<typename CFRefType>
QnCFRefHolder<CFRefType>::~QnCFRefHolder()
{
    if (m_ref)
        CFRelease(m_ref);
}

template<typename CFRefType>
CFRefType QnCFRefHolder<CFRefType>::ref()
{
    return m_ref;
}

template<typename CFRefType>
const CFRefType QnCFRefHolder<CFRefType>::ref() const
{
    return m_ref;
}

template<typename CFRefType>
void QnCFRefHolder<CFRefType>::reset(CFRefType ref)
{
    if (m_ref)
        CFRelease(m_ref);

    m_ref = ref;
}

template<typename CFRefType>
QnCFRefHolder<CFRefType>& QnCFRefHolder<CFRefType>::operator=(const QnCFRefHolder& other)
{
    reset(other.ref());
    if (m_ref)
        CFRetain(m_ref);

    return *this;
}

} // namespace cf

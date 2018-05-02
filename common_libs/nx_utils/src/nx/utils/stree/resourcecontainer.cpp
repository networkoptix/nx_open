#include "resourcecontainer.h"

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace utils {
namespace stree {

namespace detail {

class ResourceContainerConstIterator:
    public AbstractConstIterator
{
public:
    ResourceContainerConstIterator(
        const std::map<int, QVariant>& mediaStreamPameters);

    virtual bool next() override;
    virtual bool atEnd() const override;
    virtual int resID() const override;
    virtual QVariant value() const override;

private:
    const std::map<int, QVariant>& m_mediaStreamPameters;
    std::map<int, QVariant>::const_iterator m_curIter;
};

ResourceContainerConstIterator::ResourceContainerConstIterator(
    const std::map<int, QVariant>& mediaStreamPameters)
    :
    m_mediaStreamPameters(mediaStreamPameters),
    m_curIter(m_mediaStreamPameters.begin())
{
}

bool ResourceContainerConstIterator::next()
{
    if (m_curIter == m_mediaStreamPameters.end())
        return false;
    ++m_curIter;
    return m_curIter != m_mediaStreamPameters.end();
}

bool ResourceContainerConstIterator::atEnd() const
{
    return m_curIter == m_mediaStreamPameters.end();
}

int ResourceContainerConstIterator::resID() const
{
    return m_curIter->first;
}

QVariant ResourceContainerConstIterator::value() const
{
    return m_curIter->second;
}

} // namespace detail

bool ResourceContainer::getAsVariant(int resID, QVariant* const value) const
{
    auto it = m_mediaStreamPameters.find(resID);
    if (it == m_mediaStreamPameters.end())
        return false;

    *value = it->second;
    return true;
}

void ResourceContainer::put(int resID, const QVariant& value)
{
    m_mediaStreamPameters[resID] = value;
}

std::unique_ptr<AbstractConstIterator> ResourceContainer::begin() const
{
    return std::make_unique<detail::ResourceContainerConstIterator>(m_mediaStreamPameters);
}

QString ResourceContainer::toString(const nx::utils::stree::ResourceNameSet& rns) const
{
    QString str;
    for (auto resPair : m_mediaStreamPameters)
    {
        if (!str.isEmpty())
            str += QLatin1String(", ");
        str += lit("%1:%2").arg(rns.findResourceByID(resPair.first).name).arg(resPair.second.toString());
    }
    return str;
}

bool ResourceContainer::empty() const
{
    return m_mediaStreamPameters.empty();
}

//-------------------------------------------------------------------------------------------------
// class SingleResourceContainer.

SingleResourceContainer::SingleResourceContainer():
    m_resID(0)
{
}

SingleResourceContainer::SingleResourceContainer(
    int resID,
    QVariant value)
    :
    m_resID(resID),
    m_value(value)
{
}

bool SingleResourceContainer::getAsVariant(int resID, QVariant* const value) const
{
    if (resID != m_resID)
        return false;
    *value = m_value;
    return true;
}

void SingleResourceContainer::put(int resID, const QVariant& value)
{
    m_resID = resID;
    m_value = value;
}


//-------------------------------------------------------------------------------------------------
// class MultiSourceResourceReader.

MultiSourceResourceReader::MultiSourceResourceReader(
    const AbstractResourceReader& rc1,
    const AbstractResourceReader& rc2)
    :
    m_elementCount(2)
{
    m_readers[0] = &rc1;
    m_readers[1] = &rc2;
}

MultiSourceResourceReader::MultiSourceResourceReader(
    const AbstractResourceReader& rc1,
    const AbstractResourceReader& rc2,
    const AbstractResourceReader& rc3)
    :
    m_elementCount(3)
{
    m_readers[0] = &rc1;
    m_readers[1] = &rc2;
    m_readers[2] = &rc3;
}

MultiSourceResourceReader::MultiSourceResourceReader(
    const AbstractResourceReader& rc1,
    const AbstractResourceReader& rc2,
    const AbstractResourceReader& rc3,
    const AbstractResourceReader& rc4)
    :
    m_elementCount(4)
{
    m_readers[0] = &rc1;
    m_readers[1] = &rc2;
    m_readers[2] = &rc3;
    m_readers[3] = &rc4;
}

bool MultiSourceResourceReader::getAsVariant(int resID, QVariant* const value) const
{
    for (size_t i = 0; i < m_elementCount; ++i)
    {
        if (m_readers[i]->getAsVariant(resID, value))
            return true;
    }

    return false;
}

//-------------------------------------------------------------------------------------------------
// class MultiSourceResourceReader.

namespace detail {

class MultiConstIterator:
    public AbstractConstIterator
{
public:
    MultiConstIterator(
        std::unique_ptr<nx::utils::stree::AbstractConstIterator> one,
        std::unique_ptr<nx::utils::stree::AbstractConstIterator> two);

    virtual bool next() override;
    virtual bool atEnd() const override;
    virtual int resID() const override;
    virtual QVariant value() const override;

private:
    std::unique_ptr<nx::utils::stree::AbstractConstIterator> m_one;
    std::unique_ptr<nx::utils::stree::AbstractConstIterator> m_two;
};

MultiConstIterator::MultiConstIterator(
    std::unique_ptr<nx::utils::stree::AbstractConstIterator> one,
    std::unique_ptr<nx::utils::stree::AbstractConstIterator> two)
    :
    m_one(std::move(one)),
    m_two(std::move(two))
{
}

bool MultiConstIterator::next()
{
    return m_one->atEnd() ? m_two->next() : m_one->next();
}

bool MultiConstIterator::atEnd() const
{
    return m_one->atEnd() && m_two->atEnd();
}

int MultiConstIterator::resID() const
{
    return m_one->atEnd() ? m_two->resID() : m_one->resID();
}

QVariant MultiConstIterator::value() const
{
    return m_one->atEnd() ? m_two->value() : m_one->value();
}

} // namespace detail

MultiIteratableResourceReader::MultiIteratableResourceReader(
    const AbstractIteratableContainer& rc1,
    const AbstractIteratableContainer& rc2)
    :
    m_rc1(rc1),
    m_rc2(rc2)
{
}

std::unique_ptr<nx::utils::stree::AbstractConstIterator> MultiIteratableResourceReader::begin() const
{
    return std::make_unique<detail::MultiConstIterator>(m_rc1.begin(), m_rc2.begin());
}

} // namespace stree
} // namespace utils
} // namespace nx

#pragma once

#include <array>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include <QtCore/QVariant>

#include <nx/utils/uuid.h>

#include "resourcenameset.h"

namespace nx {
namespace utils {
namespace stree {

//!Implement this interface to allow reading resource values from object
class NX_UTILS_API AbstractResourceReader
{
public:
    virtual ~AbstractResourceReader() = default;

    /*!
        \return true, if resource \a resID found, false otherwise
    */
    virtual bool getAsVariant( int resID, QVariant* const value ) const = 0;
    //!Helper function to get typed value
    /*!
        If stored value cannot be cast to type \a T, false is returned
    */
    template<typename T> bool get( int resID, T* const value ) const
    {
        QVariant untypedValue;
        if (!get(resID, &untypedValue))
            return false;
        if (!untypedValue.canConvert<T>())
            return false;
        *value = untypedValue.value<T>();
        return true;
    }
    bool get( int resID, QnUuid* const value ) const
    {
        QVariant untypedValue;
        if (!get(resID, &untypedValue))
            return false;
        if (untypedValue.canConvert<QnUuid>())
        {
            *value = untypedValue.value<QnUuid>();
            return true;
        }
        if (untypedValue.canConvert<QString>())
        {
            *value = QnUuid::fromStringSafe(untypedValue.value<QString>());
            return true;
        }
        return false;
    }
    bool get(int resID, std::string* const value) const
    {
        QVariant untypedValue;
        if (!get(resID, &untypedValue))
            return false;
        if (untypedValue.canConvert<QString>())
        {
            *value = untypedValue.toString().toStdString();
            return true;
        }
        if (untypedValue.canConvert<QnUuid>())
        {
            *value = untypedValue.value<QnUuid>().toStdString();
            return true;
        }
        return false;
    }
    template<typename T> boost::optional<T> get( int resID ) const
    {
        T val;
        if( !get( resID, &val ) )
            return boost::none;
        return val;
    }
    bool get( int resID, QVariant* const value ) const
    {
        return getAsVariant( resID, value );
    }
    boost::optional<QVariant> get( int resID ) const
    {
        QVariant val;
        if( !get( resID, &val ) )
            return boost::none;
        return val;
    }
};

//!Implement this interface to allow writing resource values to object
class NX_UTILS_API AbstractResourceWriter
{
public:
    virtual ~AbstractResourceWriter() {}

    /*!
        If resource \a resID already exists, its value is overwritten with \a value. Otherwise, new resource added
    */
    virtual void put( int resID, const QVariant& value ) = 0;
};

class NX_UTILS_API AbstractConstIterator
{
public:
    virtual ~AbstractConstIterator() {}

    //!Returns \a false if already at the end
    virtual bool next() = 0;
    virtual bool atEnd() const = 0;
    virtual int resID() const = 0;
    virtual QVariant value() const = 0;
    template<typename T> T value() { return value().value<T>(); }
};

//!Implement this interface to allow walk through container data
/*!
    Usage sample:
    AbstractIteratableContainer* cont = ...;
    for( auto it = cont->begin(); !it->atEnd(); it->next() )
    {
        //accessing data through iterator
    }
*/
class NX_UTILS_API AbstractIteratableContainer
{
public:
    virtual ~AbstractIteratableContainer() {}

    //!Returns iterator set to the first element
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const = 0;
};


//!Allows to add/get resources. Represents associative container
class NX_UTILS_API ResourceContainer
:
    public AbstractResourceReader,
    public AbstractResourceWriter,
    public AbstractIteratableContainer
{
public:
    ResourceContainer();
    ResourceContainer(ResourceContainer&&);
    ResourceContainer(const ResourceContainer&);

    ResourceContainer& operator=(ResourceContainer&&);
    ResourceContainer& operator=(const ResourceContainer&);

    //!Implementation of AbstractResourceReader::get
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
    //!Implementation of AbstractResourceReader::put
    virtual void put( int resID, const QVariant& value ) override;

    //!Implementation of AbstractIteratableContainer::begin
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override;

    QString toString( const nx::utils::stree::ResourceNameSet& rns ) const;
    bool empty() const;

private:
    std::map<int, QVariant> m_mediaStreamPameters;
};

class NX_UTILS_API SingleResourceContainer
:
    public AbstractResourceReader,
    public AbstractResourceWriter
{
public:
    SingleResourceContainer();
    SingleResourceContainer(
        int resID,
        QVariant value );

    //!Implementation of AbstractResourceReader::get
    virtual bool getAsVariant( int resID, QVariant* const value ) const;
    //!Implementation of AbstractResourceReader::put
    virtual void put( int resID, const QVariant& value );

private:
    int m_resID;
    QVariant m_value;
};

//!Reads from two AbstractResourceReader
class NX_UTILS_API MultiSourceResourceReader
:
    public AbstractResourceReader
{
public:
    //TODO #ak make initializer a variadic template func
    MultiSourceResourceReader(
        const AbstractResourceReader& rc1,
        const AbstractResourceReader& rc2);
    MultiSourceResourceReader(
        const AbstractResourceReader& rc1,
        const AbstractResourceReader& rc2,
        const AbstractResourceReader& rc3);
    MultiSourceResourceReader(
        const AbstractResourceReader& rc1,
        const AbstractResourceReader& rc2,
        const AbstractResourceReader& rc3,
        const AbstractResourceReader& rc4);

    //!Implementation of AbstractResourceReader::get
    /*!
        Reads first reader. If no value found, reads second reader
    */
    virtual bool getAsVariant( int resID, QVariant* const value ) const;

private:
    const size_t m_elementCount;
    std::array<const AbstractResourceReader*, 4> m_readers;
};

//TODO #ak using variadic template add method makeMultiSourceResourceReader that accepts any number of aguments


class NX_UTILS_API MultiIteratableResourceReader
:
    public AbstractIteratableContainer
{
public:
    MultiIteratableResourceReader(
        const AbstractIteratableContainer& rc1,
        const AbstractIteratableContainer& rc2);

    //!Implementation of AbstractIteratableContainer::begin
    virtual std::unique_ptr<nx::utils::stree::AbstractConstIterator> begin() const override;

private:
    const AbstractIteratableContainer& m_rc1;
    const AbstractIteratableContainer& m_rc2;

    MultiIteratableResourceReader(const MultiIteratableResourceReader&);
    MultiIteratableResourceReader& operator=(const MultiIteratableResourceReader&);
};

/**
    * Writes to \a target and \a additionalOutput.
    */
class NX_UTILS_API ResourceWriterProxy
:
    public nx::utils::stree::AbstractResourceWriter
{
public:
    ResourceWriterProxy(
        nx::utils::stree::AbstractResourceWriter* target,
        nx::utils::stree::AbstractResourceWriter* additionalOutput)
    :
        m_target(target),
        m_additionalOutput(additionalOutput)
    {
    }

    virtual void put(int resID, const QVariant& value) override
    {
        m_target->put(resID, value);
        m_additionalOutput->put(resID, value);
    }

private:
    AbstractResourceWriter* m_target;
    AbstractResourceWriter* m_additionalOutput;
};

} // namespace stree
} // namespace utils
} // namespace nx

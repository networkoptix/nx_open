////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef RESOURCECONTAINER_H
#define RESOURCECONTAINER_H

#include <array>
#include <map>
#include <memory>

#include <boost/optional.hpp>

#include <QtCore/QVariant>

#include "resourcenameset.h"


namespace stree
{
    //!Implement this interface to allow reading resource values from object
    class AbstractResourceReader
    {
    public:
        virtual ~AbstractResourceReader() {}

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
            if( !get( resID, &untypedValue ) )
                return false;
            if( !untypedValue.canConvert<T>() )
                return false;
            *value = untypedValue.value<T>();
            return true;
        }
        template<typename T> boost::optional<T> get( int resID ) const
        {
            T val;
            if( !get<T>( resID, &val ) )
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
    class AbstractResourceWriter
    {
    public:
        virtual ~AbstractResourceWriter() {}

        /*!
            If resource \a resID already exists, its value is overwritten with \a value. Otherwise, new resource added
        */
        virtual void put( int resID, const QVariant& value ) = 0;
    };

    class AbstractConstIterator
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
    class AbstractIteratableContainer
    {
    public:
        virtual ~AbstractIteratableContainer() {}

        //!Returns iterator set to the first element
        virtual std::unique_ptr<stree::AbstractConstIterator> begin() const = 0;
    };


    //!Allows to add/get resources. Represents associative container
    class ResourceContainer
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
        virtual std::unique_ptr<stree::AbstractConstIterator> begin() const override;

        QString toString( const stree::ResourceNameSet& rns ) const;

    private:
        std::map<int, QVariant> m_mediaStreamPameters;
    };

    class SingleResourceContainer
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
    class MultiSourceResourceReader
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

        //!Implementation of AbstractResourceReader::get
        /*!
            Reads first reader. If no value found, reads second reader
        */
        virtual bool getAsVariant( int resID, QVariant* const value ) const;

    private:
        const size_t m_elementCount;
        std::array<const AbstractResourceReader*, 3> m_readers;
    };

    //TODO #ak using variadic template add method makeMultiSourceResourceReader that accepts any number of aguments


    class MultiIteratableResourceReader
    :
        public AbstractIteratableContainer
    {
    public:
        MultiIteratableResourceReader(
            const AbstractIteratableContainer& rc1,
            const AbstractIteratableContainer& rc2);

        //!Implementation of AbstractIteratableContainer::begin
        virtual std::unique_ptr<stree::AbstractConstIterator> begin() const override;

    private:
        const AbstractIteratableContainer& m_rc1;
        const AbstractIteratableContainer& m_rc2;

        MultiIteratableResourceReader(const MultiIteratableResourceReader&);
        MultiIteratableResourceReader& operator=(const MultiIteratableResourceReader&);
    };
}

#endif  //RESOURCECONTAINER_H

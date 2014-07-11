////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef RESOURCECONTAINER_H
#define RESOURCECONTAINER_H

#include <map>

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
        virtual bool get( int resID, QVariant* const value ) const = 0;
        //!Helper function to get typed value
        /*!
            If stored value cannot be cast to type \a T, false is returned
        */
        template<typename T> bool getTypedVal( int resID, T* const value ) const
        {
            QVariant untypedValue;
            if( !get( resID, &untypedValue ) )
                return false;
            if( !untypedValue.canConvert<T>() )
                return false;
            *value = untypedValue.value<T>();
            return true;
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

    //!Implement this interface to allow work through container
    class AbstractIteratableContainer
    {
    public:
        virtual ~AbstractIteratableContainer() {}

        //!Sets iternal iterator just before the first element
        virtual void goToBeginning() = 0;
        //!Goes to the next element. Returns false if already at the end
        virtual bool next() = 0;
        virtual int resID() const = 0;
        virtual QVariant value() const = 0;
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

        //!Implementation of AbstractResourceReader::get
        virtual bool get( int resID, QVariant* const value ) const override;
        //!Implementation of AbstractResourceReader::put
        virtual void put( int resID, const QVariant& value ) override;

        //!Implementation of AbstractIteratableContainer::goToBeginning
        virtual void goToBeginning() override;
        //!Implementation of AbstractIteratableContainer::next
        virtual bool next() override;
        //!Implementation of AbstractIteratableContainer::resID
        virtual int resID() const override;
        //!Implementation of AbstractIteratableContainer::value
        virtual QVariant value() const override;

        QString toString( const stree::ResourceNameSet& rns ) const;

    private:
        std::map<int, QVariant> m_mediaStreamPameters;
        std::map<int, QVariant>::const_iterator m_curIter;
        bool m_first;
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
        virtual bool get( int resID, QVariant* const value ) const;
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
        MultiSourceResourceReader( const AbstractResourceReader& rc1, const AbstractResourceReader& rc2 );

        //!Implementation of AbstractResourceReader::get
        /*!
            Reads first reader. If no value found, reads second reader
        */
        virtual bool get( int resID, QVariant* const value ) const;

    private:
        const AbstractResourceReader& m_rc1;
        const AbstractResourceReader& m_rc2;
    };
}

#endif  //RESOURCECONTAINER_H

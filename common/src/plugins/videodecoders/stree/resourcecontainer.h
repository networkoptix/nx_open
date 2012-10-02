////////////////////////////////////////////////////////////
// 27 sep 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef RESOURCECONTAINER_H
#define RESOURCECONTAINER_H

#include <map>

#include <QVariant>


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

    //!Allows to add/get resources. Represents associative container
    class ResourceContainer
    :
        public AbstractResourceReader,
        public AbstractResourceWriter
    {
    public:
        //!Implementation of AbstractResourceReader::get
        virtual bool get( int resID, QVariant* const value ) const;
        //!Implementation of AbstractResourceReader::put
        virtual void put( int resID, const QVariant& value );

    private:
        std::map<int, QVariant> m_mediaStreamPameters;
    };

    //!Reads from two AbstractResourceReader
    class MultiSourceResourceReader
    :
        public AbstractResourceReader
    {
    public:
        MultiSourceResourceReader( AbstractResourceReader* const rc1, AbstractResourceReader* const rc2 );

        //!Implementation of AbstractResourceReader::get
        /*!
            Reads first reader. If no value found, reads second reader
        */
        virtual bool get( int resID, QVariant* const value ) const;

    private:
        AbstractResourceReader* const m_rc1;
        AbstractResourceReader* const m_rc2;
    };
}

#endif  //RESOURCECONTAINER_H

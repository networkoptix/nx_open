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
        virtual bool get( int resID, QVariant* value ) const = 0;
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
        virtual bool get( int resID, QVariant* value ) const;
        //!Implementation of AbstractResourceReader::put
        virtual void put( int resID, const QVariant& value );

    private:
        std::map<int, QVariant> m_mediaStreamPameters;
    };
}

#endif  //RESOURCECONTAINER_H

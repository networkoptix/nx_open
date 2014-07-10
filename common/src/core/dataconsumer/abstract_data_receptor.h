////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef ABSTRACT_DATA_RECEPTOR_H
#define ABSTRACT_DATA_RECEPTOR_H

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/media_data_packet.h"


//!Abstract interface of class, accepting media data
/*!
    \note In a correct way we should make QnAbstractDataConsumer not thread and add another class derived from QnAbstractDataConsumer
*/
class QnAbstractDataReceptor
{
public:
    virtual ~QnAbstractDataReceptor() {}

    //!Returns true, if following \a putData call is garanteed to accept input data
    virtual bool canAcceptData() const = 0;
    //!
    /*!
        \note Can ignore data for some reasons (e.g., some internal buffer size is exceeded). 
            Data provider should use \a canAcceptData method to find out whether it is possible
    */
    virtual void putData( const QnAbstractDataPacketPtr& data ) = 0;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //ABSTRACT_DATA_RECEPTOR_H

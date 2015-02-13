/**********************************************************
* 12 feb 2015
* a.kolesnikov
***********************************************************/

#ifndef ABSTRACT_PROXYING_DATA_RECEPTOR_H
#define ABSTRACT_PROXYING_DATA_RECEPTOR_H

#ifdef ENABLE_DATA_PROVIDERS

#include <type_traits>

#include "abstract_data_receptor.h"


//!Proxies input data to another \a QnAbstractDataReceptor
/*!
    Can perform some processing on input data
*/
class QnAbstractProxyingDataReceptor
:
    public QnAbstractDataReceptor
{
public:
    QnAbstractProxyingDataReceptor() {}
    
    template<class DataReceptorRefType>
    QnAbstractProxyingDataReceptor( DataReceptorRefType&& dataReceptorRef )
    :
        m_nextReceptor( std::forward<DataReceptorRefType>( dataReceptorRef ) )
    {
    }

    //!Set receptor to be invoked after this one
    template<class DataReceptorRefType>
    void setNextReceptor( DataReceptorRefType&& dataReceptorRef )
    {
        m_nextReceptor = std::forward<DataReceptorRefType>( dataReceptorRef );
    }

protected:
    QnAbstractDataReceptorPtr m_nextReceptor;
};

#endif  //ENABLE_DATA_PROVIDERS

#endif  //ABSTRACT_PROXYING_DATA_RECEPTOR_H

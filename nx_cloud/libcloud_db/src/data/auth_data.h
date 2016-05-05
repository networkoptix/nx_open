/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef CDB_AUTH_DATA_H
#define CDB_AUTH_DATA_H

#include <plugins/videodecoder/stree/resourcecontainer.h>

#include <cloud_db_client/src/data/auth_data.h>


namespace nx {
namespace cdb {
namespace data {

class AuthRequest
:
    public api::AuthRequest,
    public stree::AbstractResourceReader
{
public:
    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

}   //data
}   //cdb
}   //nx

#endif   //CDB_AUTH_DATA_H

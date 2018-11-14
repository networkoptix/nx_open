#pragma once

#include <nx/utils/stree/resourcecontainer.h>

#include <nx/cloud/db/client/data/auth_data.h>

namespace nx::cloud::db {
namespace data {

class AuthRequest:
    public api::AuthRequest,
    public nx::utils::stree::AbstractResourceReader
{
public:
    //!Implementation of \a nx::utils::stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

} // namespace data
} // namespace nx::cloud::db

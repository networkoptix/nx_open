/**********************************************************
* Aug 6, 2015
* a.kolesnikov
***********************************************************/

#include "account_data.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <utils/common/model_functions.h>
#include <nx/network/buffer.h>

#include "stree/cdb_ns.h"


namespace nx {
namespace cdb {
namespace data {

bool AccountData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case attr::accountID:
            *value = QString::fromStdString(id);
            return true;
        case attr::accountEmail:
            *value = QString::fromStdString(email);
            return true;

        default:
            return false;
    }
}

bool AccountConfirmationCode::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}


bool AccountUpdateData::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}


AccountUpdateDataWithEmail::AccountUpdateDataWithEmail(AccountUpdateData&& rhs)
:
    AccountUpdateData(std::move(rhs))
{
}


bool AccountEmail::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::accountEmail:
            *value = QString::fromStdString(email);
            return true;

        default:
            return false;
    }
}


bool TemporaryCredentialsParams::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    return false;
}


std::string AccessRestrictions::toString() const
{
    return boost::algorithm::join(requestsAllowed, ",");
}

bool AccessRestrictions::parse(const std::string& str)
{
    using namespace boost::algorithm;

    boost::algorithm::split(
        requestsAllowed,
        str,
        is_any_of(","),
        token_compress_on);
    return true;
}

bool AccessRestrictions::authorize(const stree::AbstractResourceReader& requestAttributes) const
{
    if (requestsAllowed.empty())
        return true;    //no restrictions

    if (auto requestPath = requestAttributes.get(attr::requestPath))
    {
        return std::find(
            requestsAllowed.begin(),
            requestsAllowed.end(),
            requestPath->toString().toStdString()) != requestsAllowed.end();
    }

    return true;
}


TemporaryAccountPassword::TemporaryAccountPassword()
:
    expirationTimestampUtc(0),
    maxUseCount(0),
    useCount(0),
    isEmailCode(false)
{
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountPassword),
    (sql_record),
    _Fields)


//QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
//    (AccountUpdateDataWithEmail),
//    (sql_record),
//    _Fields)

}   //data
}   //cdb
}   //nx

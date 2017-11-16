#include "account_data.h"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>

#include <nx/fusion/model_functions.h>
#include <nx/network/buffer.h>
#include <nx/utils/timer_manager.h>

#include "../stree/cdb_ns.h"

namespace nx {
namespace cdb {
namespace data {

//-------------------------------------------------------------------------------------------------
bool AccountRegistrationData::getAsVariant(int resID, QVariant* const value) const
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

//-------------------------------------------------------------------------------------------------
AccountData::AccountData(AccountRegistrationData registrationData):
    api::AccountData(std::move(registrationData))
{
}

AccountData::AccountData(api::AccountData apiAccountData):
    api::AccountData(std::move(apiAccountData))
{
}

bool AccountData::getAsVariant( int resID, QVariant* const value ) const
{
    switch( resID )
    {
        case attr::accountId:
            *value = QString::fromStdString(id);
            return true;
        case attr::accountEmail:
            *value = QString::fromStdString(email);
            return true;

        default:
            return false;
    }
}

//-------------------------------------------------------------------------------------------------
bool AccountConfirmationCode::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}

//-------------------------------------------------------------------------------------------------
bool AccountUpdateData::getAsVariant(int /*resID*/, QVariant* const /*value*/) const
{
    //TODO #ak
    return false;
}

//-------------------------------------------------------------------------------------------------
AccountUpdateDataWithEmail::AccountUpdateDataWithEmail()
{
}

AccountUpdateDataWithEmail::AccountUpdateDataWithEmail(AccountUpdateData&& rhs):
    AccountUpdateData(std::move(rhs))
{
}

//-------------------------------------------------------------------------------------------------
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

//-------------------------------------------------------------------------------------------------
bool TemporaryCredentialsParams::getAsVariant(
    int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::credentialsExpirationPeriod:
            *value = QString::number(timeouts.expirationPeriod.count()) + "s";
            return true;

        case attr::credentialsProlongationPeriod:
            if (!timeouts.autoProlongationEnabled)
                return false;
            *value = QString::number(timeouts.prolongationPeriod.count())+"s";
            return true;
    }

    return false;
}

void TemporaryCredentialsParams::put(int resID, const QVariant& value)
{
    using namespace std::chrono;

    switch(resID)
    {
        case attr::credentialsExpirationPeriod:
            timeouts.expirationPeriod =
                duration_cast<seconds>(nx::utils::parseTimerDuration(value.toString()));
            return;

        case attr::credentialsProlongationPeriod:
            timeouts.autoProlongationEnabled = true;
            timeouts.prolongationPeriod =
                duration_cast<seconds>(nx::utils::parseTimerDuration(value.toString()));
            return;
    }
}

//-------------------------------------------------------------------------------------------------
std::string AccessRestrictions::toString() const
{
    std::string result;
    if (!requestsAllowed.empty())
        result += "+" + boost::algorithm::join(requestsAllowed, ":+");
    if (!requestsDenied.empty())
        result += "-" + boost::algorithm::join(requestsDenied, ":-");
    return result;
}

bool AccessRestrictions::parse(const std::string& str)
{
    using namespace boost::algorithm;

    //TODO #ak use spirit?

    std::vector<std::string> allRequestsWithModifiers;
    boost::algorithm::split(
        allRequestsWithModifiers,
        str,
        is_any_of(":"),
        token_compress_on);
    for (std::string& requestWithModifier: allRequestsWithModifiers)
    {
        if (requestWithModifier.empty())
        {
            NX_ASSERT(false);
            continue;
        }

        if (requestWithModifier[0] == '+')
        {
            requestsAllowed.push_back(requestWithModifier.substr(1));
        }
        else if (requestWithModifier[0] == '-')
        {
            requestsDenied.push_back(requestWithModifier.substr(1));
        }
        else
        {
            NX_ASSERT(false, requestWithModifier.c_str());
            continue;
        }
    }

    return true;
}

bool AccessRestrictions::authorize(const nx::utils::stree::AbstractResourceReader& requestAttributes) const
{
    std::string requestPath;
    if (!requestAttributes.get(attr::requestPath, &requestPath))
        return true;    //assert?

    if (!requestsAllowed.empty())
        return std::find(
            requestsAllowed.begin(),
            requestsAllowed.end(),
            requestPath) != requestsAllowed.end();

    return std::find(
        requestsDenied.begin(),
        requestsDenied.end(),
        requestPath) == requestsDenied.end();
}

//-------------------------------------------------------------------------------------------------
TemporaryAccountCredentials::TemporaryAccountCredentials():
    expirationTimestampUtc(0),
    prolongationPeriodSec(0),
    maxUseCount(0),
    useCount(0),
    isEmailCode(false)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountCredentials),
    (sql_record),
    _Fields)

} // namespace data
} // namespace cdb
} // namespace nx

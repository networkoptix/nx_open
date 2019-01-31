#include "account_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/buffer.h>
#include <nx/utils/timer_manager.h>

#include "../stree/cdb_ns.h"

namespace nx::cloud::db {
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
bool AccountUpdateData::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::ha1:
            if (!passwordHa1)
                return false;
            *value = QString::fromStdString(*passwordHa1);
            return true;

        default:
            return false;
    }
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
} // namespace nx::cloud::db

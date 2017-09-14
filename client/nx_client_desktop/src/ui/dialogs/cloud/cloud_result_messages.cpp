#include "cloud_result_messages.h"

QString QnCloudResultMessages::invalidCredentials()
{
    return tr("Incorrect email or password");
}

QString QnCloudResultMessages::accountNotActivated()
{
    return tr("Account isn't activated. Please check your email and follow provided instructions");
}

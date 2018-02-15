#include "cloud_result_messages.h"

QString QnCloudResultMessages::invalidCredentials()
{
    return tr("Incorrect email or password");
}

QString QnCloudResultMessages::accountNotActivated()
{
    return tr("Account is not activated. Please check your email and follow provided instructions");
}

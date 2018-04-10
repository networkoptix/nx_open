#include "cloud_result_messages.h"

QString QnCloudResultMessages::accountNotFound()
{
    return tr("Account not found");
}

QString QnCloudResultMessages::accountNotActivated()
{
    return tr("Account is not activated. Please check your email and follow the provided instructions.");
}

QString QnCloudResultMessages::invalidPassword()
{
    return tr("Invalid password");
}

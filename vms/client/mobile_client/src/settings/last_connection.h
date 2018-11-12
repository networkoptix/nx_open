#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <utils/common/encoded_credentials.h>

namespace nx {
namespace client {
namespace mobile {
namespace settings {

struct LastConnectionData
{
    QString systemName;
    nx::utils::Url url;
    QnEncodedCredentials credentials;

    nx::utils::Url urlWithCredentials() const;
};
#define LastConnectionData_Fields (systemName)(url)(credentials)

QN_FUSION_DECLARE_FUNCTIONS(LastConnectionData, (json))

} // namespace settings
} // namespace mobile
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::mobile::settings::LastConnectionData)

#include "ec2_email.h"

namespace ec2 {

    void ApiEmailSettingsData::fromResource( const QnEmail::Settings& settings )
    {
        host = settings.server;
        port = settings.port;
        user = settings.user;
        password = settings.password;
        connectionType = settings.connectionType;
    }

    void ApiEmailSettingsData::toResource( QnEmail::Settings& settings ) const
    {
        settings.server = host;
        settings.port = port;
        settings.user = user;
        settings.password = password;
        settings.connectionType = connectionType;
    }
}

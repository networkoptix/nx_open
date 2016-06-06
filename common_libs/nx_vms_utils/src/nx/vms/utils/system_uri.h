#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QScopedPointer>
#include <QtCore/QHash>
#include <QtCore/QString>

namespace nx
{
    namespace vms
    {
        namespace utils
        {
                /**
                * Classes for handling and constructing generic URI protocol links.
                *
                * Full scheme looks like:
                * * {protocol}://{domain}/{client_command}/{system_id}{system_action}?auth={access_key}&{action_parameters}
                *
                * @see https://networkoptix.atlassian.net/wiki/display/PM/NX+URL+Scheme
                *
                * Protocol part is customizable, so we don't parse it here.
                * Auth is encoded as base64(login:pass).
                * All query parameters are url-encoded.
                */
            class SystemUriPrivate;

            class NX_VMS_UTILS_API SystemUri
            {
            public:
                enum class Scope
                {
                    Generic,            /**< Generic url. */
                    Direct              /**< Direct-access url. */
                };

                enum class Protocol
                {
                    Http,
                    Https,
                    Native,
                };

                enum class ClientCommand
                {
                    None,
                    Client,             /**< Just open the client */
                    LoginToCloud,       /**< Login to cloud. Auth is required. */
                    ConnectToSystem,    /**< Login to system. System id and auth are required. */
                };

                enum class SystemAction
                {
                    View,               /**< Open some cameras. */
                };

                SystemUri();
                SystemUri(const QString& uri);
                virtual ~SystemUri();

                Scope scope() const;
                void setScope(Scope value);

                Protocol protocol() const;
                void setProtocol(Protocol value);

                QString domain() const;
                void setDomain(const QString& value);

                ClientCommand clientCommand() const;
                void setClientCommand(ClientCommand value);

                QString systemId() const;
                void setSystemId(const QString& value);

                SystemAction systemAction() const;
                void setSystemAction(SystemAction value);

                struct Auth
                {
                    QString user;
                    QString password;
                };
                Auth authenticator() const;
                void setAuthenticator(const Auth& value);
                void setAuthenticator(const QString& user, const QString& password);

                /** Raw parameters using is strongly discouraged. */
                typedef QHash<QString, QString> Parameters;
                Parameters rawParameters() const;
                void setRawParameters(const Parameters& value);

                void addParameter(const QString& key, const QString& value);

                bool isNull() const;

                bool isValid() const;

                QString toString() const;
                QUrl toUrl() const;

                //TODO: #GDM when fusion will be moved out to separate library, change to QnLexical
                static QString toString(SystemUri::Scope value);
                static QString toString(SystemUri::Protocol value);
                static QString toString(SystemUri::ClientCommand value);
                static QString toString(SystemUri::SystemAction value);

                bool operator==(const SystemUri& other) const;
            private:
                QScopedPointer<SystemUriPrivate> const d_ptr;
                Q_DECLARE_PRIVATE(SystemUri);
            };

        }
    }
}
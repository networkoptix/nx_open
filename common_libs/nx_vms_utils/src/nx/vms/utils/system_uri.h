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
                enum class Protocol
                {
                    Http,
                    Https,
                    Native,
                };

                enum class ClientCommand
                {
                    None,
                    Open,               /**< Just open the client */
                    LoginToCloud,       /**< Login to cloud. Auth is required. */
                    ConnectToSystem,    /**< Login to system. System id and auth are required. */
                };

                enum class SystemAction
                {
                    None,
                    View,               /**< Open some cameras. */
                };

                SystemUri();
                SystemUri(const QString& uri);
                virtual ~SystemUri();

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

                /** Raw parameters using is strongly discouraged. */
                typedef QHash<QString, QString> Parameters;
                Parameters rawParameters() const;
                void setRawParameters(const Parameters& value);

                bool isNull() const;

                QString toString() const;
                QUrl toUrl() const;
            private:
                QScopedPointer<SystemUriPrivate> const d_ptr;
                Q_DECLARE_PRIVATE(SystemUri);
            };


            /**
            * QnSystemUriResolver is a state machine, which state is a result of uri parsing.
            * Main option is a target action list - login to cloud, connect to server, etc.
            * Additional options are action parameters.
            */
            class NX_VMS_UTILS_API SystemUriResolver
            {
            public:
                enum class Action
                {
                    None,
                    LoginToCloud,       /*< Just login to cloud. Parameters: login, password. */
                    ConnectToServer     /*< Directly connect to server. Parameters: serverUrl. */
                };

                struct Result
                {
                    QList<Action> actions;
                    QUrl serverUrl;
                    QString login;
                    QString password;
                };

                SystemUriResolver();
                SystemUriResolver(const QUrl &uri);

                bool isValid() const;

                Result result() const;

                bool parseUri();
            private:
                bool m_valid;
                QUrl m_uri;
                Result m_result;
            };

        }
    }
}
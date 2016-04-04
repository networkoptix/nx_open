#pragma once

#include <QtCore/QUrl>

/**
 * Class for handling generic URI protocol.
 * For now uri may look like:
 * * nx-vms://?
 * * nx-vms://?auth=...
 * * nx-vms://system/?auth=...
 *
 * Protocol part is customizable, so we don't parse it here.
 * Auth is encoded as base64(login:pass).
 * All query parameters are url-encoded.
 *
 * QnSystemUriResolver is a state machine, which state is a result of uri parsing.
 * Main option is a target action list - login to cloud, connect to server, etc.
 * Additional options are action parameters.
 */
class QnSystemUriResolver
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

    QnSystemUriResolver();
    QnSystemUriResolver(const QUrl &uri);

    bool isValid() const;

    Result result() const;

    bool parseUri();



private:
    bool m_valid;
    QUrl m_uri;
    Result m_result;
};

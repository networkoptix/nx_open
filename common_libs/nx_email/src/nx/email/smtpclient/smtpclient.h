/*
  Copyright (c) 2011-2012 - Tőkés Attila

  This file is part of SmtpClient for Qt.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  See the LICENSE file for more details.
*/

#ifndef SMTPCLIENT_H
#define SMTPCLIENT_H

#include <QObject>

#include <nx/network/socket.h>
#include <nx/network/http/linesplitter.h>
#include <utils/email/email_fwd.h>

#include "mimemessage.h"


class SmtpClient : public QObject
{
    Q_OBJECT
public:

    /* [0] Enumerations */

    enum AuthMethod
    {
        AuthPlain,
        AuthLogin
    };

    enum ConnectionType
    {
        TcpConnection,
        SslConnection,
        TlsConnection       // STARTTLS
    };


    static QString toString( ConnectionType connectionType );

    /* [0] --- */


    /* [1] Constructors and Destructors */

    SmtpClient(const QString & host = lit("locahost"), int port = 25, ConnectionType ct = TcpConnection);

    ~SmtpClient();

    /* [1] --- */


    /* [2] Getters and Setters */

    const QString& getHost() const;
    void setHost(QString &host);

    int getPort() const;
    void setPort(int port);

    const QString& getName() const;
    void setName(const QString &name);

    ConnectionType getConnectionType() const;
    void setConnectionType(ConnectionType ct);

    const QString & getUser() const;
    void setUser(const QString &host);

    const QString & getPassword() const;
    void setPassword(const QString &password);

    SmtpClient::AuthMethod getAuthMethod() const;
    void setAuthMethod(AuthMethod method);

    const QString & getResponseText() const;
    SmtpReplyCode getResponseCode() const;

    int getConnectionTimeout() const;
    void setConnectionTimeout(int msec);

    int getResponseTimeout() const;
    void setResponseTimeout(int msec);

    int getSendMessageTimeout() const;
    void setSendMessageTimeout(int msec);


    /* [2] --- */


    /* [3] Public methods */

    SmtpOperationResult connectToHost();

    SmtpOperationResult login();
    SmtpOperationResult login(const QString &user, const QString &password, AuthMethod method = AuthLogin);

    SmtpOperationResult sendMail(const MimeMessage& email);

    void quit();


    /* [3] --- */

protected:

    /* [4] Protected members */

    std::unique_ptr<AbstractStreamSocket> m_socket;

    QString host;
    int port;
    ConnectionType connectionType;
    QString name;

    QString user;
    QString password;
    AuthMethod authMethod;

    int connectionTimeout;
    int responseTimeout;
    int sendMessageTimeout;


    QString responseText;
    SmtpReplyCode responseCode;

    nx_http::LineSplitter m_lineSpliter;

    class ResponseTimeoutException {};
    class SendMessageTimeoutException {};

    /* [4] --- */


    /* [5] Protected methods */

    void waitForResponse();

    void sendMessage(QString text);

    /* [5] --- */
};

#endif // SMTPCLIENT_H

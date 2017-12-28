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

#include "smtpclient.h"

#include <QFileInfo>
#include <QByteArray>

#include <nx/network/ssl_socket.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <utils/email/email.h>


/* [1] Constructors and destructors */

QString SmtpClient::toString(ConnectionType connectionType)
{
    switch (connectionType)
    {
        case TcpConnection:
            return lit("TcpConnection");
        case SslConnection:
            return lit("SslConnection");
        case TlsConnection:
            return lit("TlsConnection");
        default:
            NX_ASSERT(false);
            return lit("unknown");
    }
}



SmtpClient::SmtpClient(const QString & host, int port, ConnectionType connectionType)
    :
    m_socket(nullptr),
    name(lit("localhost")),
    authMethod(AuthLogin),
    connectionTimeout(5000),
    responseTimeout(5000),
    sendMessageTimeout(60000),
    responseCode(SmtpReplyCode::NoReply)
{
    setConnectionType(connectionType);

    this->host = host;
    this->port = port;
}

SmtpClient::~SmtpClient() {}

/* [1] --- */


/* [2] Getters and Setters */

void SmtpClient::setUser(const QString &user)
{
    this->user = user;
}

void SmtpClient::setPassword(const QString &password)
{
    this->password = password;
}

void SmtpClient::setAuthMethod(AuthMethod method)
{
    this->authMethod = method;
}

void SmtpClient::setHost(QString &host)
{
    this->host = host;
}

void SmtpClient::setPort(int port)
{
    this->port = port;
}

void SmtpClient::setConnectionType(ConnectionType ct)
{
    this->connectionType = ct;

    m_lineSpliter.reset();
    m_socket = nx::network::SocketFactory::createStreamSocket(connectionType == SslConnection);
}

const QString& SmtpClient::getHost() const
{
    return this->host;
}

const QString& SmtpClient::getUser() const
{
    return this->user;
}

const QString& SmtpClient::getPassword() const
{
    return this->password;
}

SmtpClient::AuthMethod SmtpClient::getAuthMethod() const
{
    return this->authMethod;
}

int SmtpClient::getPort() const
{
    return this->port;
}

SmtpClient::ConnectionType SmtpClient::getConnectionType() const
{
    return connectionType;
}

const QString& SmtpClient::getName() const
{
    return this->name;
}

void SmtpClient::setName(const QString &name)
{
    this->name = name;
}

const QString & SmtpClient::getResponseText() const
{
    return responseText;
}

SmtpReplyCode SmtpClient::getResponseCode() const
{
    return responseCode;
}

int SmtpClient::getConnectionTimeout() const
{
    return connectionTimeout;
}

void SmtpClient::setConnectionTimeout(int msec)
{
    connectionTimeout = msec;
}

int SmtpClient::getResponseTimeout() const
{
    return responseTimeout;
}

void SmtpClient::setResponseTimeout(int msec)
{
    responseTimeout = msec;
}

int SmtpClient::getSendMessageTimeout() const
{
    return sendMessageTimeout;
}

void SmtpClient::setSendMessageTimeout(int msec)
{
    sendMessageTimeout = msec;
}

/* [2] --- */


/* [3] Public methods */

SmtpOperationResult SmtpClient::connectToHost()
{
    if (!m_socket->connect(host, port, std::chrono::milliseconds(connectionTimeout)))
        return {SmtpError::ConnectionTimeoutError};

    try
    {
        // Wait for the server's response
        waitForResponse();

        // If the response code is not 220 (Service ready)
        // means that is something wrong with the server
        if (responseCode != SmtpReplyCode::ServiceReady)
            return {SmtpError::ServerError, responseCode};

        // Send a EHLO/HELO message to the server
        // The client's first command must be EHLO/HELO
        sendMessage(lit("EHLO ") + name);

        // Wait for the server's response
        waitForResponse();

        // The response code needs to be 250.
        if (responseCode != SmtpReplyCode::MailActionOK)
            return {SmtpError::ServerError, responseCode};

        if (connectionType == TlsConnection)
        {
            // send a request to start TLS handshake
            sendMessage(lit("STARTTLS"));

            // Wait for the server's response
            waitForResponse();

            // The response code needs to be 220.
            if (responseCode != SmtpReplyCode::ServiceReady)
                return {SmtpError::ServerError, responseCode};

            m_socket = std::make_unique<nx::network::deprecated::SslSocket>(
                std::move(m_socket), /*isServerSide*/ false);

            // Send ELHO one more time
            sendMessage(lit("EHLO ") + name);

            // Wait for the server's response
            waitForResponse();

            // The response code needs to be 250.
            if (responseCode != SmtpReplyCode::MailActionOK)
                return {SmtpError::ServerError, responseCode};
        }
    }
    catch (ResponseTimeoutException)
    {
        return {SmtpError::ResponseTimeoutError};
    }
    catch (SendMessageTimeoutException)
    {
        return {SmtpError::SendDataTimeoutError};
    }

    // If no errors occurred the function returns success.
    return {SmtpError::Success};
}

SmtpOperationResult SmtpClient::login()
{
    return login(user, password, authMethod);
}

SmtpOperationResult SmtpClient::login(const QString &user, const QString &password, AuthMethod method)
{
    try
    {
        if (method == AuthPlain)
        {
            // Sending command: AUTH PLAIN base64('\0' + username + '\0' + password)
            sendMessage(lit("AUTH PLAIN ") + QLatin1String(QByteArray().append((char)0).append(user).append((char)0).append(password).toBase64()));

            // Wait for the server's response
            waitForResponse();

            // If the response is not 235 then the authentication was failed
            if (responseCode != SmtpReplyCode::AuthSuccessful)
                return {SmtpError::AuthenticationFailedError, responseCode};
        }
        else if (method == AuthLogin)
        {
            // Sending command: AUTH LOGIN
            sendMessage(lit("AUTH LOGIN"));

            // Wait for 334 response code
            waitForResponse();
            if (responseCode != SmtpReplyCode::ServerChallenge)
                return {SmtpError::AuthenticationFailedError, responseCode};

            // Send the username in base64
            sendMessage(QLatin1String(QByteArray().append(user).toBase64()));

            // Wait for 334
            waitForResponse();
            if (responseCode != SmtpReplyCode::ServerChallenge)
                return {SmtpError::AuthenticationFailedError, responseCode};

            // Send the password in base64
            sendMessage(QLatin1String(QByteArray().append(password).toBase64()));

            // Wait for the server's response
            waitForResponse();

            // If the response is not 235 then the authentication was failed
            if (responseCode != SmtpReplyCode::AuthSuccessful)
                return {SmtpError::AuthenticationFailedError, responseCode};
        }
    }
    catch (ResponseTimeoutException)
    {
        return {SmtpError::ResponseTimeoutError};
    }
    catch (SendMessageTimeoutException)
    {
        return {SmtpError::SendDataTimeoutError};
    }

    // If no errors occurred the function returns success.
    return {SmtpError::Success};
}

SmtpOperationResult SmtpClient::sendMail(const MimeMessage& email)
{
    try
    {
        // Send the MAIL command with the sender
        sendMessage(lit("MAIL FROM: <") + email.getSender().getAddress() + lit(">"));

        waitForResponse();

        if (responseCode != SmtpReplyCode::MailActionOK)
            return {SmtpError::ServerError, responseCode};

        // Send RCPT command for each recipient
        QList<EmailAddress>::const_iterator it, itEnd;
        // To (primary recipients)
        for (it = email.getRecipients().begin(), itEnd = email.getRecipients().end();
            it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != SmtpReplyCode::MailActionOK)
                return {SmtpError::ServerError, responseCode};
        }

        // Cc (carbon copy)
        for (it = email.getRecipients(MimeMessage::Cc).begin(), itEnd = email.getRecipients(MimeMessage::Cc).end();
            it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != SmtpReplyCode::MailActionOK)
                return {SmtpError::ServerError, responseCode};
        }

        // Bcc (blind carbon copy)
        for (it = email.getRecipients(MimeMessage::Bcc).begin(), itEnd = email.getRecipients(MimeMessage::Bcc).end();
            it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != SmtpReplyCode::MailActionOK)
                return {SmtpError::ServerError, responseCode};
        }

        // Send DATA command
        sendMessage(lit("DATA"));
        waitForResponse();

        if (responseCode != SmtpReplyCode::StartMailInput)
            return {SmtpError::ServerError, responseCode};

        sendMessage(email.toString());

        // Send \r\n.\r\n to end the mail data
        sendMessage(lit("."));

        waitForResponse();

        if (responseCode != SmtpReplyCode::MailActionOK)
            return {SmtpError::ServerError, responseCode};
    }
    catch (ResponseTimeoutException)
    {
        return {SmtpError::ResponseTimeoutError};
    }
    catch (SendMessageTimeoutException)
    {
        return {SmtpError::SendDataTimeoutError};
    }

    // If no errors occurred the function returns success.
    return {SmtpError::Success};
}

void SmtpClient::quit()
{
    sendMessage(lit("QUIT"));
}

/* [3] --- */


/* [4] Protected methods */

void SmtpClient::waitForResponse()
{
    QByteArray readBuffer;
    readBuffer.reserve(4 * 1024);

    for (int bufPos = 0;; )
    {
        nx::network::http::ConstBufferRefType lineBufferRef;
        size_t bytesParsed = 0;
        NX_ASSERT(bufPos <= readBuffer.size());
        if (bufPos == readBuffer.size() ||
            !m_lineSpliter.parseByLines(
                nx::network::http::ConstBufferRefType(readBuffer, bufPos, readBuffer.size()),
                &lineBufferRef,
                &bytesParsed))
        {
            readBuffer.resize(readBuffer.capacity());
            const int bytesRead = m_socket->recv(readBuffer.data(), readBuffer.size());
            if (bytesRead <= 0)
            {
                NX_LOG(lit("Error receiving data from SMTP server %1. %2").arg(m_socket->getForeignAddress().toString()).
                    arg(bytesRead == 0 ? lit("Connection closed") : SystemError::getLastOSErrorText()), cl_logDEBUG1);
                throw ResponseTimeoutException();
            }
            readBuffer.resize(bytesRead);
            bufPos = 0;
            continue;
        }

        bufPos += static_cast<int>(bytesParsed);

        // Save the server's response
        responseText = QLatin1String(lineBufferRef.toByteArrayWithRawData());

        // Extract the response code from the server's response (first 3 digits)
        responseCode = static_cast<SmtpReplyCode>(responseText.left(3).toInt());

        if (responseText[3] == QLatin1Char(' '))
            break;
    }
}

void SmtpClient::sendMessage(QString text)
{
    const int bytesSent = m_socket->send(text.toUtf8() + "\r\n");
    if (bytesSent == -1)
        throw SendMessageTimeoutException();
}

/* [4] --- */


/* [5] Slots for the m_socket's signals */

//void SmtpClient::socketStateChanged(QAbstractSocket::SocketState )
//{
//}
//
//void SmtpClient::socketError(QAbstractSocket::SocketError )
//{
//}
//
//void SmtpClient::socketReadyRead()
//{
//}

/* [5] --- */





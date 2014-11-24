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

#include <utils/common/log.h>


/* [1] Constructors and destructors */

QString SmtpClient::toString( ConnectionType connectionType )
{
    switch( connectionType )
    {
        case TcpConnection:
            return lit("TcpConnection");
        case SslConnection:
            return lit("SslConnection");
        case TlsConnection:
            return lit("TlsConnection");
        default:
            assert(false);
            return lit("unknown");
    }
}



SmtpClient::SmtpClient(const QString & host, int port, ConnectionType connectionType)
:
    m_socket(nullptr),
    name(lit("localhost")),
    authMethod(AuthPlain),
    connectionTimeout(5000),
    responseTimeout(5000),
    sendMessageTimeout(60000),
    responseCode(0)
{
    setConnectionType(connectionType);

    this->host = host;
    this->port = port;

    //connect(m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)),
    //        this, SLOT(socketStateChanged(QAbstractSocket::SocketState)));
    //connect(m_socket, SIGNAL(error(QAbstractSocket::SocketError)),
    //        this, SLOT(socketError(QAbstractSocket::SocketError)));
    //connect(m_socket, SIGNAL(readyRead()),
    //        this, SLOT(socketReadyRead()));
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
    m_socket = SocketFactory::createStreamSocket( connectionType == SslConnection || connectionType == TlsConnection );
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

int SmtpClient::getResponseCode() const
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

bool SmtpClient::connectToHost()
{
    switch (connectionType)
    {
        case SslConnection:
        case TcpConnection:
            if( !m_socket->connect( host, port, connectionTimeout ) )
            {
                emit smtpError( ConnectionTimeoutError );
                return false;
            }
            break;

        case TlsConnection:
            if( !static_cast<AbstractEncryptedStreamSocket*>(m_socket)->connectWithoutEncryption( host, port, connectionTimeout ) )
            {
                emit smtpError( ConnectionTimeoutError );
                return false;
            }
            break;
    }

    try
    {
        // Wait for the server's response
        waitForResponse();

        // If the response code is not 220 (Service ready)
        // means that is something wrong with the server
        if (responseCode != 220)
        {
            emit smtpError(ServerError);
            return false;
        }

        // Send a EHLO/HELO message to the server
        // The client's first command must be EHLO/HELO
        sendMessage(lit("EHLO ") + name);

        // Wait for the server's response
        waitForResponse();

        // The response code needs to be 250.
        if (responseCode != 250) {
            emit smtpError(ServerError);
            return false;
        }

        if (connectionType == TlsConnection) {
            // send a request to start TLS handshake
            sendMessage(lit("STARTTLS"));

            // Wait for the server's response
            waitForResponse();

            // The response code needs to be 220.
            if (responseCode != 220) {
                emit smtpError(ServerError);
                return false;
            };

            if( !static_cast<AbstractEncryptedStreamSocket*>(m_socket)->enableClientEncryption() )
            {
                //qDebug() << ((QSslSocket*) socket)->errorString();
                emit smtpError(ConnectionTimeoutError);
                return false;
            }
            //((QSslSocket*) socket)->startClientEncryption();

            //if (!((QSslSocket*) socket)->waitForEncrypted(connectionTimeout)) {
            //    qDebug() << ((QSslSocket*) socket)->errorString();
            //    emit smtpError(ConnectionTimeoutError);
            //    return false;
            //}

            // Send ELHO one more time
            sendMessage(lit("EHLO ") + name);

            // Wait for the server's response
            waitForResponse();

            // The response code needs to be 250.
            if (responseCode != 250) {
                emit smtpError(ServerError);
                return false;
            }
        }
    }
    catch (ResponseTimeoutException)
    {
        return false;
    }
    catch (SendMessageTimeoutException)
    {
        return false;
    }

    // If no errors occured the function returns true.
    return true;
}

bool SmtpClient::login()
{
    return login(user, password, authMethod);
}

bool SmtpClient::login(const QString &user, const QString &password, AuthMethod method)
{
    try {
        if (method == AuthPlain)
        {
            // Sending command: AUTH PLAIN base64('\0' + username + '\0' + password)
            sendMessage(lit("AUTH PLAIN ") + QLatin1String(QByteArray().append((char) 0).append(user).append((char) 0).append(password).toBase64()));

            // Wait for the server's response
            waitForResponse();

            // If the response is not 235 then the authentication was faild
            if (responseCode != 235)
            {
                emit smtpError(AuthenticationFailedError);
                return false;
            }
        }
        else if (method == AuthLogin)
        {
            // Sending command: AUTH LOGIN
            sendMessage(lit("AUTH LOGIN"));

            // Wait for 334 response code
            waitForResponse();
            if (responseCode != 334) { emit smtpError(AuthenticationFailedError); return false; }

            // Send the username in base64
            sendMessage(QLatin1String(QByteArray().append(user).toBase64()));

            // Wait for 334
            waitForResponse();
            if (responseCode != 334) { emit smtpError(AuthenticationFailedError); return false; }

            // Send the password in base64
            sendMessage(QLatin1String(QByteArray().append(password).toBase64()));

            // Wait for the server's responce
            waitForResponse();

            // If the response is not 235 then the authentication was faild
            if (responseCode != 235)
            {
                emit smtpError(AuthenticationFailedError);
                return false;
            }
        }
    }
    catch (ResponseTimeoutException )
    {
        // Responce Timeout exceeded
        emit smtpError(AuthenticationFailedError);
        return false;
    }
    catch (SendMessageTimeoutException)
    {
	// Send Timeout exceeded
        emit smtpError(AuthenticationFailedError);
        return false;
    }

    return true;
}

bool SmtpClient::sendMail(const MimeMessage& email)
{
    try
    {
        // Send the MAIL command with the sender
        sendMessage(lit("MAIL FROM: <") + email.getSender().getAddress() + lit(">"));

        waitForResponse();

        if (responseCode != 250) return false;

        // Send RCPT command for each recipient
        QList<EmailAddress>::const_iterator it, itEnd;
        // To (primary recipients)
        for (it = email.getRecipients().begin(), itEnd = email.getRecipients().end();
             it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != 250) return false;
        }

        // Cc (carbon copy)
        for (it = email.getRecipients(MimeMessage::Cc).begin(), itEnd = email.getRecipients(MimeMessage::Cc).end();
             it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != 250) return false;
        }

        // Bcc (blind carbon copy)
        for (it = email.getRecipients(MimeMessage::Bcc).begin(), itEnd = email.getRecipients(MimeMessage::Bcc).end();
             it != itEnd; ++it)
        {
            sendMessage(lit("RCPT TO: <") + it->getAddress() + lit(">"));
            waitForResponse();

            if (responseCode != 250) return false;
        }

        // Send DATA command
        sendMessage(lit("DATA"));
        waitForResponse();

        if (responseCode != 354) return false;

        sendMessage(email.toString());

        // Send \r\n.\r\n to end the mail data
        sendMessage(lit("."));

        waitForResponse();

        if (responseCode != 250) return false;
    }
    catch (ResponseTimeoutException)
    {
        return false;
    }
    catch (SendMessageTimeoutException)
    {
        return false;
    }

    return true;
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
    readBuffer.reserve( 4*1024 );
    
    for( int bufPos = 0;; )
    {
        nx_http::ConstBufferRefType lineBufferRef;
        size_t bytesParsed = 0;
        assert( bufPos <= readBuffer.size() );
        if( bufPos == readBuffer.size() ||
            !m_lineSpliter.parseByLines(
                nx_http::ConstBufferRefType( readBuffer, bufPos, readBuffer.size() ),
                &lineBufferRef,
                &bytesParsed ) )
        {
            readBuffer.resize( readBuffer.capacity() );
            const int bytesRead = m_socket->recv( readBuffer.data(), readBuffer.size() );
            if( bytesRead <= 0 )
            {
                NX_LOG( lit( "Error receiving data from SMTP server %1. %2" ).arg( m_socket->getForeignAddress().toString() ).
                    arg( bytesRead == 0 ? lit( "Connection closed" ) : SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
                emit smtpError( ResponseTimeoutError );
                throw ResponseTimeoutException();
            }
            readBuffer.resize( bytesRead );
            bufPos = 0;
            continue;
        }

        bufPos += bytesParsed;

        // Save the server's response
        responseText = QLatin1String( lineBufferRef.toByteArrayWithRawData() );

        // Extract the respose code from the server's responce (first 3 digits)
        responseCode = responseText.left( 3 ).toInt();

        if( responseCode / 100 == 4 )
            emit smtpError( ServerError );

        if( responseCode / 100 == 5 )
            emit smtpError( ClientError );

        if( responseText[3] == QLatin1Char( ' ' ) ) { return; }

        

        //if (!m_socket->waitForReadyRead(responseTimeout))
        //{
        //    emit smtpError(ResponseTimeoutError);
        //    throw ResponseTimeoutException();
        //}

        //while (m_socket->canReadLine()) {
        //    // Save the server's response
        //    responseText = QLatin1String(m_socket->readLine());

        //    // Extract the respose code from the server's responce (first 3 digits)
        //    responseCode = responseText.left(3).toInt();

        //    if (responseCode / 100 == 4)
        //        emit smtpError(ServerError);

        //    if (responseCode / 100 == 5)
        //        emit smtpError(ClientError);

        //    if (responseText[3] == QLatin1Char(' ')) { return; }
        //}
    } while (true);
}

void SmtpClient::sendMessage(QString text)
{
    const int bytesSent = m_socket->send( text.toUtf8() + "\r\n" );
    if( bytesSent == -1 )
    //m_socket->write(text.toUtf8() + "\r\n");
    //if (! m_socket->waitForBytesWritten(sendMessageTimeout))
    {
      emit smtpError(SendDataTimeoutError);
      throw SendMessageTimeoutException();
    }
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





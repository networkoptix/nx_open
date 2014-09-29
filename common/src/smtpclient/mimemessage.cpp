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

#include "mimemessage.h"

#include <QDateTime>
#include "quotedprintable.h"
#include <typeinfo>

/* [1] Constructors and Destructors */
MimeMessage::MimeMessage()
:
    hEncoding(MimePart::_8Bit)
{
    m_content.reset( new MimeMultiPart() );
}

/* [1] --- */


/* [2] Getters and Setters */
MimePart& MimeMessage::getContent() {
    return *m_content;
}

void MimeMessage::setContent(MimePart* _content) {
    m_content.reset( _content );
}

void MimeMessage::setSender(EmailAddress&& e)
{
    m_sender = std::move(e);
}

void MimeMessage::addRecipient(EmailAddress&& rcpt, RecipientType type)
{
    switch (type)
    {
    case To:
        addTo( std::move(rcpt) );
        break;
    case Cc:
        addCc( std::move(rcpt) );
        break;
    case Bcc:
        addBcc( std::move(rcpt) );
        break;
    }
}

void MimeMessage::addTo(EmailAddress&& rcpt) {
    m_recipientsTo.push_back(std::move(rcpt));
}

void MimeMessage::addCc(EmailAddress&& rcpt) {
    m_recipientsTo.push_back(std::move(rcpt));
}

void MimeMessage::addBcc(EmailAddress&& rcpt) {
    m_recipientsTo.push_back(std::move(rcpt));
}

void MimeMessage::setSubject(const QString & subject)
{
    this->subject = subject;
}

void MimeMessage::addPart(MimePart *part) noexcept
{
    if (typeid(*m_content) == typeid(MimeMultiPart)) {
        ((MimeMultiPart*) m_content.get())->addPart(part);
    } else {
        delete part;
    }
}

void MimeMessage::setHeaderEncoding(MimePart::Encoding hEnc)
{
    this->hEncoding = hEnc;
}

const EmailAddress & MimeMessage::getSender() const
{
    return m_sender;
}

const QList<EmailAddress> & MimeMessage::getRecipients(RecipientType type) const
{
    switch (type)
    {
    default:
    case To:
        return m_recipientsTo;
    case Cc:
        return m_recipientsCc;
    case Bcc:
        return m_recipientsBcc;
    }
}

const QString & MimeMessage::getSubject() const
{
    return subject;
}

/* [2] --- */


/* [3] Public Methods */

QString MimeMessage::toString() const
{
    QString mime;

    /* =========== MIME HEADER ============ */

    /* ---------- Sender / From ----------- */
    mime = lit("From:");
    if (m_sender.getName() != lit(""))
    {
        switch (hEncoding)
        {
        case MimePart::Base64:
            mime += lit(" =?utf-8?B?") + QLatin1String(QByteArray().append(m_sender.getName()).toBase64()) + lit("?=");
            break;
        case MimePart::QuotedPrintable:
            mime += lit(" =?utf-8?Q?") + QuotedPrintable::encode(QByteArray().append(m_sender.getName())).replace(QLatin1Char(' '), lit("_")).replace(QLatin1Char(':'), lit("=3A")) + lit("?=");
            break;
        default:
            mime += lit(" ") + m_sender.getName();
        }
    }
    mime += lit(" <") + m_sender.getAddress() + lit(">\r\n");
    /* ---------------------------------- */


    /* ------- Recipients / To ---------- */    
    mime += lit("To:");
    QList<EmailAddress>::const_iterator it;  int i;
    for (i = 0, it = m_recipientsTo.begin(); it != m_recipientsTo.end(); ++it, ++i)
    {
        if (i != 0) { mime += lit(","); }

        if (it->getName() != lit(""))
        {
            switch (hEncoding)
            {
            case MimePart::Base64:
                mime += lit(" =?utf-8?B?") + QLatin1String(QByteArray().append(it->getName()).toBase64()) + lit("?=");
                break;
            case MimePart::QuotedPrintable:
                mime += lit(" =?utf-8?Q?") + QuotedPrintable::encode(QByteArray().append(it->getName())).replace(QLatin1Char(' '), lit("_")).replace(QLatin1Char(':'), lit("=3A")) + lit("?=");
                break;
            default:
                mime += lit(" ") + it->getName();
            }
        }
        mime += lit(" <") + it->getAddress() + lit(">");
    }
    mime += lit("\r\n");
    /* ---------------------------------- */

    /* ------- Recipients / Cc ---------- */
    if (m_recipientsCc.size() != 0) {
        mime += lit("Cc:");
    }
    for (i = 0, it = m_recipientsCc.begin(); it != m_recipientsCc.end(); ++it, ++i)
    {
        if (i != 0) { mime += lit(","); }

        if (it->getName() != lit(""))
        {
            switch (hEncoding)
            {
            case MimePart::Base64:
                mime += lit(" =?utf-8?B?") + QLatin1String(QByteArray().append(it->getName()).toBase64()) + lit("?=");
                break;
            case MimePart::QuotedPrintable:
                mime += lit(" =?utf-8?Q?") + QuotedPrintable::encode(QByteArray().append(it->getName())).replace(QLatin1Char(' '), lit("_")).replace(QLatin1Char(':'),lit("=3A")) + lit("?=");
                break;
            default:
                mime += lit(" ") + it->getName();
            }
        }
        mime += lit(" <") + it->getAddress() + lit(">");
    }
    if (m_recipientsCc.size() != 0) {
        mime += lit("\r\n");
    }
    /* ---------------------------------- */

    /* ------------ Subject ------------- */
    mime += lit("Subject: ");


    switch (hEncoding)
    {
    case MimePart::Base64:
        mime += lit("=?utf-8?B?") + QLatin1String(QByteArray().append(subject).toBase64()) + lit("?=");
        break;
    case MimePart::QuotedPrintable:
        mime += lit("=?utf-8?Q?") + QuotedPrintable::encode(QByteArray().append(subject)).replace(QLatin1Char(' '), lit("_")).replace(QLatin1Char(':'), lit("=3A")) + lit("?=");
        break;
    default:
        mime += subject;
    }
    /* ---------------------------------- */

    mime += lit("\r\n");
    mime += lit("MIME-Version: 1.0\r\n");

    mime += m_content->toString();
    return mime;
}

/* [3] --- */

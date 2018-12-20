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
MimeMessage::MimeMessage():
    m_content(new MimeMultiPart()),
    hEncoding(MimePart::_8Bit)
{
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
    if (auto multiPart = dynamic_cast<MimeMultiPart*>(part))
        multiPart->addPart(part);
    else
        delete part;
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
    mime = "From:";
    if (!m_sender.getName().isEmpty())
    {
        switch (hEncoding)
        {
            case MimePart::Base64:
                mime += " =?utf-8?B?";
                mime += QLatin1String(QByteArray().append(m_sender.getName()).toBase64());
                mime += "?=";
                break;
            case MimePart::QuotedPrintable:
                mime += " =?utf-8?Q?";
                mime += QuotedPrintable::encode(QByteArray().append(m_sender.getName()))
                    .replace(' ', '_').replace(':', "=3A");
                mime += "?=";
                break;
            default:
                mime += ' ' + m_sender.getName();
        }
    }
    mime += " <" + m_sender.getAddress() + ">\r\n";
    /* ---------------------------------- */


    /* ------- Recipients / To ---------- */    
    mime += "To:";
    QList<EmailAddress>::const_iterator it;
    int i;
    for (i = 0, it = m_recipientsTo.begin(); it != m_recipientsTo.end(); ++it, ++i)
    {
        if (i != 0)
            mime += ",";

        if (!it->getName().isEmpty())
        {
            switch (hEncoding)
            {
                case MimePart::Base64:
                    mime += " =?utf-8?B?";
                    mime += QLatin1String(QByteArray().append(it->getName()).toBase64());
                    mime += "?=";
                    break;
                case MimePart::QuotedPrintable:
                    mime += " =?utf-8?Q?";
                    mime += QuotedPrintable::encode(QByteArray().append(it->getName()))
                        .replace(' ', '_').replace(':', "=3A");
                    mime += "?=";
                    break;
                default:
                    mime += ' ' + it->getName();
            }
        }
        mime += " <" + it->getAddress() + ">";
    }
    mime += "\r\n";
    /* ---------------------------------- */

    /* ------- Recipients / Cc ---------- */
    if (!m_recipientsCc.isEmpty())
    {
        mime += "Cc:";

        for (i = 0, it = m_recipientsCc.begin(); it != m_recipientsCc.end(); ++it, ++i)
        {
            if (i != 0)
                mime += ",";

            if (!it->getName().isEmpty())
            {
                switch (hEncoding)
                {
                    case MimePart::Base64:
                        mime += " =?utf-8?B?";
                        mime += QLatin1String(QByteArray().append(it->getName()).toBase64());
                        mime += "?=";
                        break;
                    case MimePart::QuotedPrintable:
                        mime += " =?utf-8?Q?";
                        mime += QuotedPrintable::encode(QByteArray().append(it->getName()))
                            .replace(' ', '_').replace(':', "=3A");
                        mime += "?=";
                        break;
                    default:
                        mime += ' ' + it->getName();
                }
            }
            mime += " <" + it->getAddress() + ">";
        }

        mime += "\r\n";
    }
    /* ---------------------------------- */

    /* ------------ Subject ------------- */
    mime += "Subject: ";

    switch (hEncoding)
    {
        case MimePart::Base64:
            mime += "=?utf-8?B?";
            mime += QLatin1String(QByteArray().append(subject).toBase64());
            mime += "?=";
            break;
        case MimePart::QuotedPrintable:
            mime += "=?utf-8?Q?";
            mime += QuotedPrintable::encode(QByteArray().append(subject))
                .replace(' ', "_").replace(':', "=3A");
            mime += "?=";
            break;
        default:
            mime += subject;
    }
    /* ---------------------------------- */

    mime += "\r\n";
    mime += "MIME-Version: 1.0\r\n";

    mime += m_content->toString();

    return mime;
}

/* [3] --- */

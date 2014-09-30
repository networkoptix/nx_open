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

#ifndef MIMEMESSAGE_H
#define MIMEMESSAGE_H

#include <memory>

#include "mimepart.h"
#include "mimemultipart.h"
#include "emailaddress.h"
#include <QList>
#include <QtCore/QSharedPointer>

class MimeMessage
{
public:

    enum RecipientType {
        To,                 // primary
        Cc,                 // carbon copy
        Bcc                 // blind carbon copy
    };

    /* [1] Constructors and Destructors */

    MimeMessage();

    /* [1] --- */


    /* [2] Getters and Setters */

    /*!
        \param e ownership of object is passed to this class
    */
    void setSender(EmailAddress&& e);
    void addRecipient(EmailAddress&& rcpt, RecipientType type = To);
    void addTo(EmailAddress&& rcpt);
    void addCc(EmailAddress&& rcpt);
    void addBcc(EmailAddress&& rcpt);
    void setSubject(const QString & subject);
    /*!
        \param rcpt ownership of object is passed to this class
    */
    void addPart(MimePart* part) noexcept;

    void setHeaderEncoding(MimePart::Encoding);

    const EmailAddress & getSender() const;
    const QList<EmailAddress> & getRecipients(RecipientType type = To) const;
    const QString & getSubject() const;

    MimePart& getContent();
    /*!
        \param _content ownership of object is passed to this class
    */
    void setContent(MimePart* _content);
    /* [2] --- */


    /* [3] Public methods */

    virtual QString toString() const;

    /* [3] --- */

private:

    /* [4] Protected members */

    EmailAddress m_sender;
    QList<EmailAddress> m_recipientsTo, m_recipientsCc, m_recipientsBcc;
    QString subject;
    std::unique_ptr<MimePart> m_content;
    
    MimePart::Encoding hEncoding;

    /* [4] --- */


};

typedef QSharedPointer<MimeMessage> MimeMessagePtr;

#endif // MIMEMESSAGE_H

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

#include "mimepart.h"
#include "quotedprintable.h"

/* [1] Constructors and Destructors */

MimePart::MimePart()
{
    cEncoding = _7Bit;
    prepared = false;
    cBoundary = lit("");
}

/* [1] --- */


/* [2] Getters and Setters */

void MimePart::setContent(const QByteArray & content)
{
    this->content = content;
}

void MimePart::setHeader(const QString & header)
{
    this->header = header;
}

void MimePart::addHeaderLine(const QString & line)
{
    this->header += line + lit("\r\n");
}

const QString& MimePart::getHeader() const
{
    return header;
}

const QByteArray& MimePart::getContent() const
{
    return content;
}

void MimePart::setContentId(const QString & cId)
{
    this->cId = cId;
}

const QString & MimePart::getContentId() const
{
    return this->cId;
}

void MimePart::setContentName(const QString & cName)
{
    this->cName = cName;
}

const QString & MimePart::getContentName() const
{
    return this->cName;
}

void MimePart::setContentType(const QString & cType)
{
    this->cType = cType;
}

const QString & MimePart::getContentType() const
{
    return this->cType;
}

void MimePart::setCharset(const QString & charset)
{
    this->cCharset = charset;
}

const QString & MimePart::getCharset() const
{
    return this->cCharset;
}

void MimePart::setEncoding(Encoding enc)
{
    this->cEncoding = enc;
}

MimePart::Encoding MimePart::getEncoding() const
{
    return this->cEncoding;
}

MimeContentFormatter& MimePart::getContentFormatter()
{
    return this->formatter;
}

/* [2] --- */


/* [3] Public methods */

QString MimePart::toString()
{
    if (!prepared)
        prepare();

    return mimeString;
}

/* [3] --- */


/* [4] Protected methods */

void MimePart::prepare()
{
    mimeString = QString();

    /* === Header Prepare === */

    /* Content-Type */
    mimeString.append(lit("Content-Type: ")).append(cType);

    if (cName != lit(""))
        mimeString.append(lit("; name=\"")).append(cName).append(lit("\""));

    if (cCharset != lit(""))
        mimeString.append(lit("; charset=")).append(cCharset);

    if (cBoundary != lit(""))
        mimeString.append(lit("; boundary=")).append(cBoundary);

    mimeString.append(lit("\r\n"));
    /* ------------ */

    /* Content-Transfer-Encoding */
    mimeString.append(lit("Content-Transfer-Encoding: "));
    switch (cEncoding)
    {
    case _7Bit:
        mimeString.append(lit("7bit\r\n"));
        break;
    case _8Bit:
        mimeString.append(lit("8bit\r\n"));
        break;
    case Base64:
        mimeString.append(lit("base64\r\n"));
        break;
    case QuotedPrintable:
        mimeString.append(lit("quoted-printable\r\n"));
        break;
    }
    /* ------------------------ */

    /* Content-Id */
    if (!cId.isEmpty())
        mimeString.append(lit("Content-ID: <")).append(cId).append(lit(">\r\n"));
    /* ---------- */

    /* Addition header lines */

    mimeString.append(header).append(lit("\r\n"));

    /* ------------------------- */

    /* === End of Header Prepare === */

    /* === Content === */
    switch (cEncoding)
    {
    case _7Bit:
        mimeString.append(QLatin1String(content));
        break;
    case _8Bit:
        mimeString.append(QLatin1String(content));
        break;
    case Base64:
        mimeString.append(formatter.format(QLatin1String(content.toBase64())));
        break;
    case QuotedPrintable:
        mimeString.append(formatter.format(QuotedPrintable::encode(content), true));
        break;
    }
    mimeString.append(lit("\r\n"));
    /* === End of Content === */

    prepared = true;
}

/* [4] --- */

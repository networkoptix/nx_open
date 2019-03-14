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
#pragma once

#include <QtCore/QObject>

#include "mimecontentformatter.h"

class MimePart: public QObject
{
    Q_OBJECT

public:
    enum Encoding
    {
        _7Bit,
        _8Bit,
        Base64,
        QuotedPrintable
    };

    MimePart();

    const QString& getHeader() const;
    const QByteArray& getContent() const;

    void setContent(const QByteArray& content);
    void setHeader(const QString& header);

    void addHeaderLine(const QString& line);

    void setContentId(const QString& cId);
    const QString& getContentId() const;

    void setContentName(const QString& cName);
    const QString& getContentName() const;

    void setContentType(const QString& cType);
    const QString& getContentType() const;

    void setCharset(const QString& charset);
    const QString& getCharset() const;

    void setEncoding(Encoding enc);
    Encoding getEncoding() const;

    MimeContentFormatter& getContentFormatter();

    virtual QString toString();

    virtual void prepare();

protected:
    QString header;
    QByteArray content;

    QString cId;
    QString cName;
    QString cType;
    QString cCharset;
    QString cBoundary;
    Encoding cEncoding = _7Bit;

    QString mimeString;
    bool prepared = false;

    MimeContentFormatter formatter;
};

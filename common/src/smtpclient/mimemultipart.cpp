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

#include "mimemultipart.h"
#include <QTime>
#include <QCryptographicHash>

const QString MULTI_PART_NAMES[] = {
    QString(QLatin1String("multipart/mixed")),         //    Mixed
    QString(QLatin1String("multipart/digest")),        //    Digest
    QString(QLatin1String("multipart/alternative")),   //    Alternative
    QString(QLatin1String("multipart/related")),       //    Related
    QString(QLatin1String("multipart/report")),        //    Report
    QString(QLatin1String("multipart/signed")),        //    Signed
    QString(QLatin1String("multipart/encrypted"))      //    Encrypted
};

MimeMultiPart::MimeMultiPart(MultiPartType type)
{
    this->type = type;
    this->cType = MULTI_PART_NAMES[this->type];
    this->cEncoding = _8Bit;

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(QByteArray().append(qrand()));
    cBoundary = QLatin1String(md5.result().toHex());
}

MimeMultiPart::~MimeMultiPart() {

}

void MimeMultiPart::addPart(MimePart *part) {
    parts.append(part);
}

const QList<MimePart*> & MimeMultiPart::getParts() const {
    return parts;
}

void MimeMultiPart::prepare() {
    QList<MimePart*>::iterator it;

    content = "";
    for (it = parts.begin(); it != parts.end(); it++) {
        content += lit("--") + cBoundary + lit("\r\n");
        (*it)->prepare();
        content += (*it)->toString();
    };

    content += lit("--") + cBoundary + lit("--\r\n");

    MimePart::prepare();
}

void MimeMultiPart::setMimeType(const MultiPartType type) {
    this->type = type;
    this->cType = MULTI_PART_NAMES[type];
}

MimeMultiPart::MultiPartType MimeMultiPart::getMimeType() const {
    return type;
}

/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtMultimedia module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/


#ifndef QNAUDIOFORMAT_H
#define QNAUDIOFORMAT_H

#ifdef Q_OS_WIN
#   include <QtMultimedia/QAudioFormat>
#   define QnAudioFormat QAudioFormat
#else

#define CUSTOM_AUDIO_FORMAT_CLASS

#include <QtCore/qobject.h>
#include <QtCore/qglobal.h>
#include <QtCore/qshareddata.h>



QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE

QT_MODULE(Multimedia)


class QnAudioFormatPrivate;

// TODO: #Elric move to contrib?

class QnAudioFormat
{
public:
    enum SampleType { Unknown, SignedInt, UnSignedInt, Float };
    enum Endian { BigEndian = QSysInfo::BigEndian, LittleEndian = QSysInfo::LittleEndian };

    QnAudioFormat();
    QnAudioFormat(const QnAudioFormat &other);
    ~QnAudioFormat();

    QnAudioFormat& operator=(const QnAudioFormat &other);
    bool operator==(const QnAudioFormat &other) const;
    bool operator!=(const QnAudioFormat &other) const;

    bool isValid() const;

    void setSampleRate(int sampleRate);
    int sampleRate() const;

    void setChannels(int channels);
    int channels() const;
    void setChannelCount(int channelCount);
    int channelCount() const;

    void setSampleSize(int sampleSize);
    int sampleSize() const;

    void setCodec(const QString &codec);
    QString codec() const;

    void setByteOrder(QnAudioFormat::Endian byteOrder);
    QnAudioFormat::Endian byteOrder() const;

    void setSampleType(QnAudioFormat::SampleType sampleType);
    QnAudioFormat::SampleType sampleType() const;

    qint64 durationForBytes(int bytes) const;
    int bytesForDuration(qint64 durationUs) const;

    int bytesPerFrame() const;

private:
    QSharedDataPointer<QnAudioFormatPrivate> d;
};


QT_END_NAMESPACE

QT_END_HEADER

#endif // !Q_OS_WIN
#endif  // QNAUDIOFORMAT_H

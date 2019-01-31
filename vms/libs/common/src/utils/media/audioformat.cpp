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


#include <QtCore/QDebug>
#include "audioformat.h"

#ifdef CUSTOM_AUDIO_FORMAT_CLASS

QT_BEGIN_NAMESPACE


class QnAudioFormatPrivate : public QSharedData
{
public:
    QnAudioFormatPrivate()
    {
        frequency = -1;
        channels = -1;
        sampleSize = -1;
        byteOrder = QnAudioFormat::Endian(QSysInfo::ByteOrder);
        sampleType = QnAudioFormat::Unknown;
    }

    QnAudioFormatPrivate(const QnAudioFormatPrivate &other):
        QSharedData(other),
        codec(other.codec),
        byteOrder(other.byteOrder),
        sampleType(other.sampleType),
        frequency(other.frequency),
        channels(other.channels),
        sampleSize(other.sampleSize)
    {
    }

    QnAudioFormatPrivate& operator=(const QnAudioFormatPrivate &other)
    {
        codec = other.codec;
        byteOrder = other.byteOrder;
        sampleType = other.sampleType;
        frequency = other.frequency;
        channels = other.channels;
        sampleSize = other.sampleSize;

        return *this;
    }

    QString codec;
    QnAudioFormat::Endian byteOrder;
    QnAudioFormat::SampleType sampleType;
    int frequency;
    int channels;
    int sampleSize;
};

/*!
    \class QnAudioFormat
    \brief The QnAudioFormat class stores audio parameter information.

    \inmodule QtMultimedia
    \ingroup  multimedia
    \since 4.6

    An audio format specifies how data in an audio stream is arranged,
    i.e, how the stream is to be interpreted. The encoding itself is
    specified by the codec() used for the stream.

    In addition to the encoding, QnAudioFormat contains other
    parameters that further specify how the audio data is arranged.
    These are the frequency, the number of channels, the sample size,
    the sample type, and the byte order. The following table describes
    these in more detail.

    \table
        \header
            \o Parameter
            \o Description
        \row
            \o Sample Rate
            \o Samples per second of audio data in Hertz.
        \row
            \o Number of channels
            \o The number of audio channels (typically one for mono
               or two for stereo)
        \row
            \o Sample size
            \o How much data is stored in each sample (typically 8
               or 16 bits)
        \row
            \o Sample type
            \o Numerical representation of sample (typically signed integer,
               unsigned integer or float)
        \row
            \o Byte order
            \o Byte ordering of sample (typically little endian, big endian)
    \endtable

    You can obtain audio formats compatible with the audio device used
    through functions in QAudioDeviceInfo. This class also lets you
    query available parameter values for a device, so that you can set
    the parameters yourself. See the QAudioDeviceInfo class
    description for details. You need to know the format of the audio
    streams you wish to play. Qt does not set up formats for you.
*/

/*!
    Construct a new audio format.

    Values are initialized as follows:
    \list
    \o sampleRate()  = -1
    \o channelCount() = -1
    \o sampleSize() = -1
    \o byteOrder()  = QnAudioFormat::Endian(QSysInfo::ByteOrder)
    \o sampleType() = QnAudioFormat::Unknown
    \c codec()      = ""
    \endlist
*/

QnAudioFormat::QnAudioFormat():
    d(new QnAudioFormatPrivate)
{
}

/*!
    Construct a new audio format using \a other.
*/

QnAudioFormat::QnAudioFormat(const QnAudioFormat &other):
    d(other.d)
{
}

/*!
    Destroy this audio format.
*/

QnAudioFormat::~QnAudioFormat()
{
}

/*!
    Assigns \a other to this QnAudioFormat implementation.
*/

QnAudioFormat& QnAudioFormat::operator=(const QnAudioFormat &other)
{
    d = other.d;
    return *this;
}

/*!
  Returns true if this QnAudioFormat is equal to the \a other
  QnAudioFormat; otherwise returns false.

  All elements of QnAudioFormat are used for the comparison.
*/

bool QnAudioFormat::operator==(const QnAudioFormat &other) const
{
    return d->frequency == other.d->frequency &&
            d->channels == other.d->channels &&
            d->sampleSize == other.d->sampleSize &&
            d->byteOrder == other.d->byteOrder &&
            d->codec == other.d->codec &&
            d->sampleType == other.d->sampleType;
}

/*!
  Returns true if this QnAudioFormat is not equal to the \a other
  QnAudioFormat; otherwise returns false.

  All elements of QnAudioFormat are used for the comparison.
*/

bool QnAudioFormat::operator!=(const QnAudioFormat& other) const
{
    return !(*this == other);
}

/*!
    Returns true if all of the parameters are valid.
*/

bool QnAudioFormat::isValid() const
{
    return d->frequency != -1 && d->channels != -1 && d->sampleSize != -1 &&
            d->sampleType != QnAudioFormat::Unknown && !d->codec.isEmpty();
}

/*!
   Sets the sample rate to \a samplerate Hertz.

   \since 4.7
*/

void QnAudioFormat::setSampleRate(int samplerate)
{
    d->frequency = samplerate;
}

/*!
    Returns the current sample rate in Hertz.

    \since 4.7
*/

int QnAudioFormat::sampleRate() const
{
    return d->frequency;
}

/*!
   Sets the channel count to \a channels.

   \since 4.7
*/

void QnAudioFormat::setChannelCount(int channels)
{
    d->channels = channels;
}

/*!
   \obsolete

   Use setChannelCount() instead.
*/

void QnAudioFormat::setChannels(int channels)
{
    d->channels = channels;
}

/*!
    Returns the current channel count value.

    \since 4.7
*/

int QnAudioFormat::channelCount() const
{
    return d->channels;
}

/*!
    \obsolete

    Use channelCount() instead.
*/

int QnAudioFormat::channels() const
{
    return d->channels;
}

/*!
   Sets the sample size to the \a sampleSize specified.
*/

void QnAudioFormat::setSampleSize(int sampleSize)
{
    d->sampleSize = sampleSize;
}

/*!
    Returns the current sample size value.
*/

int QnAudioFormat::sampleSize() const
{
    return d->sampleSize;
}

/*!
   Sets the codec to \a codec.

   \sa QAudioDeviceInfo::supportedCodecs()
*/

void QnAudioFormat::setCodec(const QString &codec)
{
    d->codec = codec;
}

/*!
    Returns the current codec value.

   \sa QAudioDeviceInfo::supportedCodecs()
*/

QString QnAudioFormat::codec() const
{
    return d->codec;
}

/*!
   Sets the byteOrder to \a byteOrder.
*/

void QnAudioFormat::setByteOrder(QnAudioFormat::Endian byteOrder)
{
    d->byteOrder = byteOrder;
}

/*!
    Returns the current byteOrder value.
*/

QnAudioFormat::Endian QnAudioFormat::byteOrder() const
{
    return d->byteOrder;
}

/*!
   Sets the sampleType to \a sampleType.
*/

void QnAudioFormat::setSampleType(QnAudioFormat::SampleType sampleType)
{
    d->sampleType = sampleType;
}

/*!
    Returns the current SampleType value.
*/

QnAudioFormat::SampleType QnAudioFormat::sampleType() const
{
    return d->sampleType;
}

qint64 QnAudioFormat::durationForBytes(int bytes) const
{
    if (!isValid() || bytes <= 0)
        return 0;

    // We round the byte count to ensure whole frames
    return qint64(1000000LL * (bytes / bytesPerFrame())) / sampleRate();
}

int QnAudioFormat::bytesForDuration(qint64 durationUs) const
{
    if (!isValid() || durationUs <= 0)
        return 0;

    return int((durationUs * bytesPerFrame() * sampleRate()) / 1000000LL);
}

int QnAudioFormat::bytesPerFrame() const
{
    if (!isValid())
        return 0;

    return (sampleSize() * channelCount()) / 8;
}


/*!
    \enum QnAudioFormat::SampleType

    \value Unknown       Not Set
    \value SignedInt     samples are signed integers
    \value UnSignedInt   samples are unsigned intergers
    \value Float         samples are floats
*/

/*!
    \enum QnAudioFormat::Endian

    \value BigEndian     samples are big endian byte order
    \value LittleEndian  samples are little endian byte order
*/

QDebug operator<<(QDebug dbg, QnAudioFormat::SampleType type)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    switch (type) {
    case QnAudioFormat::SignedInt:
        dbg << "SignedInt";
        break;
    case QnAudioFormat::UnSignedInt:
        dbg << "UnSignedInt";
        break;
    case QnAudioFormat::Float:
        dbg << "Float";
        break;
    default:
        dbg << "Unknown";
        break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const QnAudioFormat &f)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "QAudioFormat(" << f.sampleRate() << "Hz, "
        << f.sampleSize() << "bit, channelCount=" << f.channelCount()
        << ", sampleType=" << f.sampleType() << ", byteOrder=" << f.byteOrder()
        << ", codec=" << f.codec() << ')';

    return dbg;
}

QT_END_NAMESPACE

#endif // CUSTOM_AUDIO_FORMAT_CLASS

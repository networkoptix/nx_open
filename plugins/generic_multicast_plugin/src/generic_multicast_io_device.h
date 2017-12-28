#pragma once

#include <memory>

#include <QtCore/QUrl>
#include <QtCore/QIODevice>

#include <nx/network/abstract_socket.h>

class GenericMulticastIoDevice: public QIODevice
{
public:
    explicit GenericMulticastIoDevice(const QUrl& url);
    virtual ~GenericMulticastIoDevice();

    virtual bool open(QIODevice::OpenMode openMode) override;
    virtual bool isSequential() const override;

protected:
    virtual qint64 readData(char* buf, qint64 maxlen) override;
    virtual qint64 writeData(const char* buf, qint64 size) override;

private:
    bool initSocket(const QUrl& url);
    qint64 cutRtpHeaderOff(
        const char* inBuf,
        qint64 inBufLength,
        char* const outBuf,
        qint64 outBufLength);

private:
    QUrl m_url;
    std::unique_ptr<nx::network::AbstractDatagramSocket> m_socket;
};

#ifndef QnServerPushDataProvider_h2055
#define QnServerPushDataProvider_h2055

#include "dataprovider/streamdataprovider.h"
#include "datapacket/mediadatapacket.h"


class QnServerPushDataProvider : public QnStreamDataProvider
{
public:
    QnServerPushDataProvider(QnResource* dev );
    virtual ~QnServerPushDataProvider(){stop();}
protected:
    virtual QnAbstractMediaDataPacketPtr getNextData() = 0;
    virtual void openStream() = 0;
    virtual void closeStream() = 0;
    virtual bool isStreamOpened() const = 0;

private:
    void run(); // in a loop: takes data from device and puts into queue
};

#endif //QnServerPushDataProvider_h2055

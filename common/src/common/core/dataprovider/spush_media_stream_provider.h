#ifndef QnServerPushDataProvider_h2055
#define QnServerPushDataProvider_h2055

#include "media_streamdataprovider.h"

class QnServerPushDataProvider : public QnAbstractMediaStreamDataProvider
{
public:
    QnServerPushDataProvider(QnResourcePtr res);
    virtual ~QnServerPushDataProvider(){stop();}
protected:

    void sleepIfNeeded();
    virtual void afterRun();
    virtual bool beforeGetData();

    virtual void openStream() = 0;
    virtual void closeStream() = 0;
    virtual bool isStreamOpened() const = 0;

private:

};

#endif //QnServerPushDataProvider_h2055

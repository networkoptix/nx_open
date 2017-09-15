#pragma once

#include <nx/client/desktop/radass/radass_fwd.h>

class QnCamDisplay;
class QnArchiveStreamReader;

namespace nx {
namespace client {
namespace desktop {

class RadassController: public QObject
{
    Q_OBJECT
public:
    RadassController();
    ~RadassController();

    void registerConsumer(QnCamDisplay* display);
    void unregisterConsumer(QnCamDisplay* display);
    int consumerCount() const;

    RadassMode mode(QnCamDisplay* display) const;
    void setMode(QnCamDisplay* display, RadassMode mode);

public slots:
    /** Inform controller that not enough data or CPU for stream */
    void onSlowStream(QnArchiveStreamReader* reader);

    /** Inform controller that no more problem with stream */
    void streamBackToNormal(QnArchiveStreamReader* reader);

private:
    void onTimer();

private:
    struct Private;
    Private* d;
};

} // namespace desktop
} // namespace client
} // namespace nx

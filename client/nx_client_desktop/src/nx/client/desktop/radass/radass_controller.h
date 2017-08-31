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

    void registerConsumer(QnCamDisplay* display);
    void unregisterConsumer(QnCamDisplay* display);
    int consumerCount() const;

//     void setMode(RadassMode mode);
//     RadassMode getMode() const;

public slots:
    /** Inform controller that not enough data or CPU for stream */
    void onSlowStream(QnArchiveStreamReader* reader);

    /** Inform controller that no more problem with stream */
    void streamBackToNormal(QnArchiveStreamReader* reader);

private:
    void onTimer();

private:
    /** try LQ->HQ once more */
    void addHQTry();

private:
    struct Private;
    QScopedPointer<Private> d;

private:
    /**
     * Rearrange items quality: put small items to LQ state, large to HQ.
     */
    void optimizeItemsQualityBySize();
};

} // namespace desktop
} // namespace client
} // namespace nx

#pragma once

#include <QtCore/QObject>
#include <core/resource/resource_consumer.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <nx/media/abstract_metadata_consumer.h>
#include <nx/vms/client/desktop/camera/camera_fwd.h>

#include <utils/common/connective.h>

class QnAbstractArchiveStreamReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class QnResourceVideoLayout;
class QnCamDisplay;
class QnLongRunnable;
class QnAbstractRenderer;
class QnClientVideoCamera;

namespace nx {
namespace utils {
class CounterWithSignal;
} // namespace utils
} // namespace nx

class QnResourceDisplay: public Connective<QObject>, protected QnResourceConsumer {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    /**
     * Constructor.
     *
     * \param resource                  Resource that this display is associated with. Must not be NULL.
     * \param parent                    Parent of this display.
     */
    QnResourceDisplay(const QnResourcePtr& resource, QObject* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~QnResourceDisplay();

    /**
     * Called while corresponding QOpenGLWidget and QOpenGLContext are still alive
     * to properly release OGL resources.
     */
    void beforeDestroy();

    /**
     * \returns                         Resource associated with this display.
     */
    const QnResourcePtr &resource() const;

    /**
     * \returns                         Media resource associated with this display, if any.
     */
    const QnMediaResourcePtr &mediaResource() const;

    /**
     * \returns                         Data provider associated with this display.
     */
    QnAbstractStreamDataProvider *dataProvider() const;

    /**
     * \returns                         Media data provider associated with this display, if any.
     */
    QnAbstractMediaStreamDataProvider *mediaProvider() const;

    /**
     * \returns                         Archive reader associated with this display, if any.
     */
    QnAbstractArchiveStreamReader* archiveReader() const;

    /**
     * \returns                         Video camera associated with this display, if any.
     */
    QnClientVideoCamera *camera() const;

    /**
     * \returns                         Camdisplay for this display, if any.
     */
    QnCamDisplay *camDisplay() const;

    /**
     * \returns                         Video resource layout, if any,
     */
    QnConstResourceVideoLayoutPtr videoLayout() const;

    /**
     * \returns                         Length of this display, in microseconds. If the length is not defined, returns -1.
     */
    qint64 lengthUSec() const;

    /**
     * \returns                         Current time of this display, in microseconds. If the time is not defined, returns -1.
     */
    qint64 currentTimeUSec() const;

    /**
     * \returns                         Whether this display is paused.
     */
    bool isPaused();

    void start();

    void play();

    void pause();

    bool isStillImage() const;

    /**
     * \param renderer                  Renderer to assign to this display. Ownership of the renderer is transferred to this display.
     */
    void addRenderer(QnAbstractRenderer *renderer);
    void removeRenderer(QnAbstractRenderer *renderer);

    void addMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);
    void removeMetadataConsumer(const nx::media::AbstractMetadataConsumerPtr& metadataConsumer);

protected:
    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    void cleanUp(QnLongRunnable *runnable) const;

private:
    /** Media resource. */
    QnMediaResourcePtr m_mediaResource;

    /** Data provider for the associated resource. */
    QPointer<QnAbstractStreamDataProvider> m_dataProvider;

    /** Media data provider. */
    QPointer<QnAbstractMediaStreamDataProvider> m_mediaProvider;

    /** Archive data provider. */
    QPointer<QnAbstractArchiveStreamReader> m_archiveReader;

    /** Video camera. */
    QPointer<QnClientVideoCamera> m_camera; // TODO: #Elric Compatibility layer. Remove.

    /** Whether this display was started. */
    bool m_started;

    QPointer<nx::utils::CounterWithSignal> m_counter;
};

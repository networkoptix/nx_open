#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>
#include <utils/math/math.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"

namespace {
    qreal gradToRad(qreal x) { return x * M_PI / 180.0; }
    qreal radToGrad(qreal x) { return x * 180.0 / M_PI; }

    /**
     * \param fovDegreees               Width-based FOV in degrees.
     * \returns                         Width-based 35mm-equivalent focal length.
     */
    qreal fovTo35mmEquiv(qreal fovRad) {
        return (36.0 / 2.0) / std::tan((fovRad / 2.0));
    }

    /**
     * \param mm35Equiv                 Width-based 35mm-equivalent focal length.
     * \returns                         Width-based FOV in degrees.
     */
    qreal mm35EquivToFov(qreal mm35Equiv) {
        return std::atan((36.0 / 2.0) / mm35Equiv) * 2.0;
    }

} // anonymous namespace

/**
 * A thread-safe interface for accessing camera's PTZ functions.
 * 
 * Note that most of the functions of this interface return integer status codes,
 * with 0 for success and non-zero for failure.
 * 
 * Also note that standard PTZ space refers to degrees for pan, tilt and fov.
 */
class QnAbstractPtzController: public QObject { // TODO: rename QnPtzController
    Q_OBJECT
public:
    /**
     * \param resource                  Resource that this ptz controller belongs to.
     */
    QnAbstractPtzController(const QnResourcePtr &resource);
    virtual ~QnAbstractPtzController();

    /**
     * \returns                         Resource that this ptz controller belongs to.
     */
    const QnResourcePtr &resource() const { return m_resource; }

    /**
     * \returns                         PTZ features that this controller implements.
     */
    virtual Qn::PtzCapabilities getCapabilities() = 0;

    /**
     * Starts PTZ movement. Speed is specified in image-based coordinate space and
     * all of its components are expected to be in range <tt>[-1, 1]</tt>. 
     * This means that implementation must handle flipped / mirrored state of 
     * the video stream. 
     *
     * \param speed                     Movement speed. 
     * \returns                         Status code.
     */
    virtual int startMove(const QVector3D &speed) = 0;

    /**
     * Stops PTZ movement.
     * 
     * \returns                         Status code.
     */
    virtual int stopMove() = 0;

    /**
     * \param[out] flip                 Flipped state of the camera's video stream.
     * \returns                         Status code.
     */
    virtual int getFlip(Qt::Orientations *flip) = 0;

    /**
     * Sets camera PTZ position. If this controller has 
     * <tt>Qn::LogicalPositionSpaceCapability<tt>, then position is expected to
     * be in standard PTZ space. Otherwise it is expected to be in device-specific
     * coordinates and thus only positions returned from a call to <tt>getPosition</tt>
     * can be safely used.
     *
     * \param position                  Position to move to.
     * \returns                         Status code.
     */
    virtual int setPosition(const QVector3D &position) = 0;

    /**
     * Gets PTZ position from camera. If this controller has 
     * <tt>Qn::LogicalPositionSpaceCapability<tt>, then position is returned in 
     * standard PTZ space. Otherwise it's returned in device-specific coordinates.
     *
     * \param[out] position             Current ptz position. 
     * \returns                         Status code.
     */
    virtual int getPosition(QVector3D *position) = 0;

    /**
     * Gets PTZ limits of the camera in standard PTZ space. 
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::LogicalPositionSpaceCapability<tt>.
     * 
     * \param[out] limits               Ptz limits.
     * \returns                         Status code.
     */
    virtual int getLimits(QnPtzLimits *limits) = 0;

    /**
     * Moves camera's viewport relative to current viewport. New viewport 
     * coordinates are provided in a coordinate space where current viewport
     * is a square with side 2 centered at <tt>(0, 0)</tt>.
     * 
     * This function is expected to be implemented only if this controller has
     * <tt>Qn::ScreenSpaceMovementCapability</tt>.
     * 
     * \param viewport                  New viewport position.
     * \returns                         Status code.
     */
    virtual int relativeMove(const QRectF &viewport) = 0;

protected:
    QnResourcePtr m_resource;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H

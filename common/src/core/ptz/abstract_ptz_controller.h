#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/resource/resource_fwd.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"
#include "ptz_preset.h"
#include "ptz_tour.h"

/**
 * A thread-safe interface for accessing camera's PTZ functions.
 * 
 * Note that standard PTZ space refers to degrees for pan, tilt and fov.
 */
class QnAbstractPtzController: public QObject {
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
     * \returns                         PTZ capabilities that this controller implements.
     */
    virtual Qn::PtzCapabilities getCapabilities() = 0;

    /**
     * \param capabilities              Capabilities to check.
     * \returns                         Whether this controller implements the given capabilities.
     */
    bool hasCapabilities(Qn::PtzCapabilities capabilities) { return (getCapabilities() & capabilities) == capabilities; }

    /**
     * Starts PTZ movement. Speed is specified in image-based coordinate space and
     * all of its components are expected to be in range <tt>[-1, 1]</tt>. 
     * This means that implementation must handle flipped / mirrored state of 
     * the video stream. 
     * 
     * \param speed                     Movement speed. 
     * \returns                         Whether the operation was successful.
     */
    virtual bool continuousMove(const QVector3D &speed) = 0;

    /**
     * Sets camera PTZ position in the given coordinate space.
     * 
     * If this camera has <tt>Qn::DevicePositioningPtzCapability</tt>, then
     * device-specific coordinate space must be supported. Note that for
     * this coordinate space only positions returned from a call to 
     * <tt>getPosition</tt> can be safely used with this function.
     * 
     * If this camera has <tt>Qn::LogicalPositioningPtzCapability</tt>,
     * then logical coordinate space must be supported.
     *
     * \param space                     Coordinate space of the provided position.
     * \param position                  Position to move to.
     * \returns                         Whether the operation was successful.
     */
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position) = 0;

    /**
     * Moves camera's viewport relative to current viewport. New viewport 
     * coordinates are provided in a coordinate space where current viewport
     * is a square with side 1 with top-left at <tt>(0, 0)</tt>.
     * 
     * This function is expected to be implemented only if this controller has
     * <tt>Qn::ViewportPositioningPtzCapability</tt>.
     * 
     * \param aspectRatio               Actual aspect ratio of the current viewport.
     * \param viewport                  New viewport position.
     * \returns                         Whether the operation was successful.
     */
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport) = 0;

    /**
     * Gets PTZ position from camera in the given coordinate space.
     *
     * \param space                     Coordinate space to get position in.
     * \param[out] position             Current ptz position. 
     * \returns                         Whether the operation was successful.
     * \see absoluteMove
     */
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) = 0;

    /**
     * Gets PTZ limits of the camera in standard PTZ space. 
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::LimitsPtzCapability<tt>.
     * 
     * \param space                     Coordinate space to get limits in.
     * \param[out] limits               Ptz limits.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getLimits(Qn::PtzCoordinateSpace space, QnPtzLimits *limits) = 0;

    /**
     * Returns the camera streams's flipped state. This function can be used for
     * implementing emulated viewport movement.
     * 
     * This function is expected to be implemented only if this controller has
     * <tt>Qn::FlipPtzCapability</tt>.
     *
     * \param[out] flip                 Flipped state of the camera's video stream.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getFlip(Qt::Orientations *flip) = 0;

    /**
     * Saves current PTZ position as a preset, either as a new one or 
     * replacing an existing one. 
     * 
     * If <tt>id</tt> of the provided preset is not set, a new <tt>id</tt> 
     * will be generated.
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PtzPresetCapability<tt>.
     *
     * \param preset                    Preset to create.
     * \param[out] presetId             Id of the created preset.
     * \returns                         Whether the operation was successful.
     */
    virtual bool createPreset(const QnPtzPreset &preset, QString *presetId) = 0;

    /**
     * Updates the given preset without changing its associated position.
     *
     * Currently this function can only be used to change preset's name.
     * 
     * \param preset                    Preset to update.
     * \returns                         Whether the operation was successful.
     */
    virtual bool updatePreset(const QnPtzPreset &preset) = 0;

    /**
     * Removes the given preset.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PtzPresetCapability<tt>.
     * 
     * \param presetId                  Id of the preset to remove.
     * \returns                         Whether the operation was successful.
     */
    virtual bool removePreset(const QString &presetId) = 0;

    /**
     * Activates the given preset.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PtzPresetCapability<tt>.
     * 
     * \param presetId                  Id of the preset to activate.
     * \returns                         Whether the operation was successful.
     */
    virtual bool activatePreset(const QString &presetId) = 0;

    /**
     * Gets a list of all PTZ presets for the camera.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PtzPresetCapability<tt>.
     * 
     * \param[out] presets              PTZ presets.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getPresets(QnPtzPresetList *presets) = 0;

    virtual bool createTour(const QnPtzTour &tour, QString *tourId) = 0;
    virtual bool removeTour(const QString &tourId) = 0;
    virtual bool activateTour(const QString &tourId) = 0;
    virtual bool getTours(QnPtzTourList *tours) = 0;

protected:
    QnResourcePtr m_resource;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H

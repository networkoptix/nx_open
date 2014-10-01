#ifndef QN_ABSTRACT_PTZ_CONTROLLER_H
#define QN_ABSTRACT_PTZ_CONTROLLER_H

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <utils/common/connective.h>

#include <core/resource/resource_fwd.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"
#include "ptz_preset.h"
#include "ptz_tour.h"
#include "ptz_data.h"
#include "ptz_math.h"
#include "ptz_object.h"
#include "ptz_auxilary_trait.h"

/**
 * A thread-safe blocking interface for accessing camera's PTZ functions.
 */
class QnAbstractPtzController: public Connective<QObject> {
    Q_OBJECT
public:
    /**
     * \param resource                  Resource that this PTZ controller belongs to.
     */
    QnAbstractPtzController(const QnResourcePtr &resource);
    virtual ~QnAbstractPtzController();

    /**
     * \returns                         Resource that this PTZ controller belongs to.
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
     * \param command                   Ptz command to check.
     * \returns                         Whether this controller supports the given command.
     */
    bool supports(Qn::PtzCommand command);

    /**
     * Starts or stops continuous PTZ movement. 
     * 
     * Speed is specified in image-based coordinate space and all of its 
     * components are expected to be in range <tt>[-1, 1]</tt>. This means that 
     * implementation must handle flipped / mirrored state of the video stream. 
     * 
     * Passing zero in speed should stop PTZ movement.
     * 
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Qn::ContinuousPtzCapabilities</tt>.
     * 
     * \param speed                     Movement speed. 
     * \returns                         Whether the operation was successful.
     */
    virtual bool continuousMove(const QVector3D &speed) = 0;

    /**
     * Starts or stops continuous focus movement.
     * 
     * Speed is specified in device-specific coordinate space and is expected 
     * to be in range <tt>[-1, 1]</tt>. Positive speed is for far focus. 
     * 
     * Passing zero should stop focus movement.
     * 
     * This function is expected to be implemented if this controller has
     * <tt>Qn::ContinuousFocusCapability</tt>.
     * 
     * \param speed                     Focus speed.
     * \returns                         Whether the operation was successful.
     */
    virtual bool continuousFocus(qreal speed) = 0;

    /**
     * Sets camera PTZ position in the given coordinate space. 
     * 
     * Note that for the function to succeed, this controller must have a 
     * capability corresponding to the provided coordinate space, 
     * that is <tt>Qn::DevicePositioningPtzCapability</tt> or 
     * <tt>Qn::LogicalPositioningPtzCapability</tt>.
     * 
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Qn::AbsolutePtzCapabilities</tt>.
     *
     * \param space                     Coordinate space of the provided position.
     * \param position                  Position to move to.
     * \param speed                     Movement speed, in range [0, 1].
     * \returns                         Whether the operation was successful.
     */
    virtual bool absoluteMove(Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed) = 0;

    /**
     * Moves camera's viewport relative to current viewport. New viewport 
     * coordinates are provided in a coordinate space where current viewport
     * is a square with side 1 with top-left at <tt>(0, 0)</tt>.
     * 
     * This function is expected to be implemented only if this controller has
     * <tt>Qn::ViewportPtzCapability</tt>.
     * 
     * \param aspectRatio               Actual aspect ratio of the current viewport.
     * \param viewport                  New viewport position.
     * \param speed                     Movement speed, in range [0, 1].
     * \returns                         Whether the operation was successful.
     */
    virtual bool viewportMove(qreal aspectRatio, const QRectF &viewport, qreal speed) = 0;

    /**
     * Gets PTZ position from camera in the given coordinate space.
     *
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Qn::AbsolutePtzCapabilities</tt>.
     *
     * \param space                     Coordinate space to get position in.
     * \param[out] position             Current ptz position. 
     * \returns                         Whether the operation was successful.
     * \see absoluteMove
     */
    virtual bool getPosition(Qn::PtzCoordinateSpace space, QVector3D *position) = 0;

    /**
     * Gets PTZ limits of the camera. 
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
     * replacing an existing one. Note that id of the provided preset
     * must be set. 
     * 
     * If you want to create a new preset, a good idea would be to set its id to 
     * <tt>QnUuid::createUuid().toString()</tt>.
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PresetsPtzCapability<tt>.
     *
     * \param preset                    Preset to create.
     * \returns                         Whether the operation was successful.
     */
    virtual bool createPreset(const QnPtzPreset &preset) = 0;

    /**
     * Updates the given preset without changing its associated position.
     * Currently this function can only be used to change preset name.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PresetsPtzCapability<tt>.
     * 
     * \param preset                    Preset to update.
     * \returns                         Whether the operation was successful.
     */
    virtual bool updatePreset(const QnPtzPreset &preset) = 0;

    /**
     * Removes the given preset.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PresetsPtzCapability<tt>.
     * 
     * \param presetId                  Id of the preset to remove.
     * \returns                         Whether the operation was successful.
     */
    virtual bool removePreset(const QString &presetId) = 0;

    /**
     * Activates the given preset.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PresetsPtzCapability<tt>.
     * 
     * \param presetId                  Id of the preset to activate.
     * \param speed                     Movement speed, in range [0, 1].
     * \returns                         Whether the operation was successful.
     */
    virtual bool activatePreset(const QString &presetId, qreal speed) = 0;

    /**
     * Gets a list of all PTZ presets for the camera.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::PresetsPtzCapability<tt>.
     * 
     * \param[out] presets              PTZ presets.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getPresets(QnPtzPresetList *presets) = 0;

    /**
     * Saves the given tour either as a new one, or replacing an existing one. 
     * Note that id of the provided preset must be set. 
     * 
     * If you want to create a new preset, a good idea would be to set its id to 
     * <tt>QnUuid::createUuid().toString()</tt>.
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::ToursPtzCapability<tt>.
     *
     * \param tour                      Tour to create.
     * \returns                         Whether the operation was successful.
     */
    virtual bool createTour(const QnPtzTour &tour) = 0;
    
    /**
     * Removes the given tour.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::ToursPtzCapability<tt>.
     * 
     * \param tourId                    Id of the tour to remove.
     * \returns                         Whether the operation was successful.
     */
    virtual bool removeTour(const QString &tourId) = 0;
    
    /**
     * Activates the given tour.
     * 
     * Note that after the tour has been started, any movement command issued 
     * to the controller will stop that tour.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::ToursPtzCapability<tt>.
     * 
     * \param tourId                    Id of the tour to activate.
     * \returns                         Whether the operation was successful.
     */
    virtual bool activateTour(const QString &tourId) = 0;

    /**
     * Gets a list of all PTZ tours for the camera.
     *
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::ToursPtzCapability<tt>.
     * 
     * \param[out] tours                PTZ tours.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getTours(QnPtzTourList *tours) = 0;

    virtual bool getActiveObject(QnPtzObject *activeObject) = 0;

    /**
     * Updates PTZ home position for the camera.
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::HomePtzCapability<tt>.
     * 
     * \param homeObject                PTZ home object.
     * \returns                         Whether the operation was successful.
     */
    virtual bool updateHomeObject(const QnPtzObject &homeObject) = 0;

    /**
     * Gets PTZ home position that is currently assigned for the camera.
     * 
     * This function is expected to be implemented only if this controller has 
     * <tt>Qn::HomePtzCapability<tt>.
     * 
     * \param[out] homePosition         PTZ home object.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getHomeObject(QnPtzObject *homeObject) = 0;

    virtual bool getAuxilaryTraits(QnPtzAuxilaryTraitList *auxilaryTraits) = 0;

    virtual bool runAuxilaryCommand(const QnPtzAuxilaryTrait &trait, const QString &data) = 0;

    /**
     * Gets all PTZ data associated with this controller in a single operation.
     * Default implementation just calls all the accessor functions one by one.
     * 
     * \param query                     Data fields to get.
     * \param[out] data                 PTZ data.
     * \returns                         Whether the operation was successful.
     */
    virtual bool getData(Qn::PtzDataFields query, QnPtzData *data);

signals:
    void changed(Qn::PtzDataFields fields);
    void finished(Qn::PtzCommand command, const QVariant &data);

protected:
    static Qn::PtzCommand spaceCommand(Qn::PtzCommand command, Qn::PtzCoordinateSpace space);

private:
    QnResourcePtr m_resource;
};

#endif // QN_ABSTRACT_PTZ_CONTROLLER_H

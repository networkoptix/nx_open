#pragma once

#include <QtCore/QObject>

#include <common/common_globals.h>

#include <core/ptz/ptz_fwd.h>
#include <core/ptz/ptz_limits.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/ptz/ptz_data.h>
#include <core/ptz/ptz_math.h>
#include <core/ptz/ptz_object.h>
#include <core/ptz/ptz_auxiliary_trait.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

#include <nx/core/ptz/vector.h>
#include <nx/core/ptz/options.h>

/**
 * A thread-safe blocking interface for accessing camera's PTZ functions.
 */
class QnAbstractPtzController: public Connective<QObject>
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    static const qreal MaxPtzSpeed;

    /**
     * @param resource Resource that this PTZ controller belongs to.
     */
    QnAbstractPtzController(const QnResourcePtr& resource);
    virtual ~QnAbstractPtzController();

    /**
     * @returns Resource that this PTZ controller belongs to.
     */
    virtual QnResourcePtr resource() const;

    /**
     * This method will be invoked after the controller is moved to the executor thread.
     * Note: invocation is done through the event loop in the executor thread.
     */
    virtual void initialize();

public slots: //< Class is exposed to QML. All functions in section below are invokable
    /**
     * @param capabilities Capabilities to check.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether this controller implements the given capabilities.
     */
    bool hasCapabilities(
        Ptz::Capabilities capabilities,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const;

    /**
     * @returns PTZ capabilities that this controller implements.
     * @param options Additional options (e.g. ptz type)
     */
    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const = 0;

    /**
     * @param command Ptz command to check.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether this controller supports the given command.
     */
    bool supports(
        Qn::PtzCommand command,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const;

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
     * at least one of the <tt>Ptz::ContinuousPtrzCapabilities</tt>.
     *
     * @param speed Movement speed.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Starts or stops continuous focus movement.
     *
     * Speed is specified in device-specific coordinate space and is expected
     * to be in range <tt>[-1, 1]</tt>. Positive speed is for far focus.
     *
     * Passing zero should stop focus movement.
     *
     * This function is expected to be implemented if this controller has
     * <tt>Ptz::ContinuousFocusCapability</tt>.
     *
     * @param speed Focus speed.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool continuousFocus(
        qreal speed,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Sets camera PTZ position in the given coordinate space.
     *
     * Note that for the function to succeed, this controller must have a
     * capability corresponding to the provided coordinate space,
     * that is <tt>Ptz::DevicePositioningPtzCapability</tt> or
     * <tt>Ptz::LogicalPositioningPtzCapability</tt>.
     *
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Ptz::AbsolutePtrzCapabilities</tt>.
     *
     * @param space Coordinate space of the provided position.
     * @param position Position to move to.
     * @param speed Movement speed, in range [0, 1].
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Moves camera's viewport relative to current viewport. New viewport
     * coordinates are provided in a coordinate space where current viewport
     * is a square with side 1 with top-left at <tt>(0, 0)</tt>.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::ViewportPtzCapability</tt>.
     *
     * @param aspectRatio Actual aspect ratio of the current viewport.
     * @param viewpor New viewport position.
     * @param speed Movement speed, in range [0, 1].
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Moves the camera relative to its current position.
     *
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Ptz::RelativePtrzCapabilities</tt>.
     *
     * @param direction Direction to move. Each component must be in range [-1, 1].
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Changes the camera focus relatively to its current position.
     *
     * This function is expected to be implemented if this controller has
     * <tt>Ptz::RelativeFocusCapability</tt>.
     *
     * @param direction. Direction to move, must be in range [-1, 1].
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool relativeFocus(
        qreal direction,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Gets PTZ position from camera in the given coordinate space.
     *
     * This function is expected to be implemented if this controller has
     * at least one of the <tt>Ptz::AbsolutePtzCapabilities</tt>.
     *
     * @param space Coordinate space to get position in.
     * @param[out] position Current ptz position.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     * @see absoluteMove
     */
    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* outPosition,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const = 0;

    /**
     * Gets PTZ limits of the camera.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::LimitsPtzCapability<tt>.
     *
     * @param space Coordinate space to get limits in.
     * @param[out] limits Ptz limits.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const = 0;

    /**
     * Returns the camera streams's flipped state. This function can be used for
     * implementing emulated viewport movement.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::FlipPtzCapability</tt>.
     *
     * @param[out] flip Flipped state of the camera's video stream.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getFlip(
        Qt::Orientations* flip,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const = 0;

    /**
     * Saves current PTZ position as a preset, either as a new one or
     * replacing an existing one. Note that id of the provided preset
     * must be set.
     *
     * If you want to create a new preset, a good idea would be to set its id to
     * <tt>QnUuid::createUuid().toString()</tt>.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::PresetsPtzCapability<tt>.
     *
     * @param preset Preset to create.
     * @returns Whether the operation was successful.
     */
    virtual bool createPreset(const QnPtzPreset& preset) = 0;

    /**
     * Updates the given preset without changing its associated position.
     * Currently this function can only be used to change preset name.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::PresetsPtzCapability<tt>.
     *
     * @param preset Preset to update.
     * @returns Whether the operation was successful.
     */
    virtual bool updatePreset(const QnPtzPreset& preset) = 0;

    /**
     * Removes the given preset.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::PresetsPtzCapability<tt>.
     *
     * @param presetId Id of the preset to remove.
     * @returns Whether the operation was successful.
     */
    virtual bool removePreset(const QString& presetId) = 0;

    /**
     * Activates the given preset.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::PresetsPtzCapability<tt>.
     *
     * @param presetId Id of the preset to activate.
     * @param speed Movement speed, in range [0, 1].
     * @returns Whether the operation was successful.
     */
    virtual bool activatePreset(const QString& presetId, qreal speed) = 0;

    /**
     * Gets a list of all PTZ presets for the camera.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::PresetsPtzCapability<tt>.
     *
     * @param[out] presets PTZ presets.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getPresets(QnPtzPresetList* presets) const = 0;

    /**
     * Saves the given tour either as a new one, or replacing an existing one.
     * Note that id of the provided preset must be set.
     *
     * If you want to create a new preset, a good idea would be to set its id to
     * <tt>QnUuid::createUuid().toString()</tt>.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::ToursPtzCapability<tt>.
     *
     * @param tour Tour to create.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool createTour(const QnPtzTour& tour) = 0;

    /**
     * Removes the given tour.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::ToursPtzCapability<tt>.
     *
     * @param tourId Id of the tour to remove.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool removeTour(const QString& tourId) = 0;

    /**
     * Activates the given tour.
     *
     * Note that after the tour has been started, any movement command issued
     * to the controller will stop that tour.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::ToursPtzCapability<tt>.
     *
     * @param tourId Id of the tour to activate.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool activateTour(const QString& tourId) = 0;

    /**
     * Gets a list of all PTZ tours for the camera.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::ToursPtzCapability<tt>.
     *
     * @param[out] tours PTZ tours.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getTours(QnPtzTourList* tours) const = 0;

    virtual bool getActiveObject(QnPtzObject* activeObject) const = 0;

    /**
     * Updates PTZ home position for the camera.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::HomePtzCapability<tt>.
     *
     * @param homeObject PTZ home object.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool updateHomeObject(const QnPtzObject& homeObject) = 0;

    /**
     * Gets PTZ home position that is currently assigned for the camera.
     *
     * This function is expected to be implemented only if this controller has
     * <tt>Ptz::HomePtzCapability<tt>.
     *
     * @param[out] homePosition PTZ home object.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getHomeObject(QnPtzObject* homeObject) const = 0;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList* auxiliaryTraits,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const = 0;

    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) = 0;

    /**
     * Gets all PTZ data associated with this controller in a single operation.
     * Default implementation just calls all the accessor functions one by one.
     *
     * @param query Data fields to get.
     * @param[out] data PTZ data.
     * @param options Additional options (e.g. ptz type)
     * @returns Whether the operation was successful.
     */
    virtual bool getData(
        Qn::PtzDataFields query,
        QnPtzData* data,
        const nx::core::ptz::Options& options = {nx::core::ptz::Type::operational}) const;

signals:
    void changed(Qn::PtzDataFields fields);
    void finished(Qn::PtzCommand command, const QVariant& data);

protected:
    static Qn::PtzCommand spaceCommand(Qn::PtzCommand command, Qn::PtzCoordinateSpace space);

private:
    QnResourcePtr m_resource;
};

bool deserialize(const QString& value, QnPtzObject* target);

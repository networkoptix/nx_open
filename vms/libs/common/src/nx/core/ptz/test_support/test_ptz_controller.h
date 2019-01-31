#pragma once

#include <functional>

#include <core/ptz/abstract_ptz_controller.h>

#include <nx/utils/std/optional.h>
#include <nx/utils/signature_extractor.h>

namespace nx {
namespace core {
namespace ptz {
namespace test_support {

#define FUNC_TYPE(METHOD) std::function<typename nx::utils::meta::SignatureExtractor<\
    decltype(&QnAbstractPtzController::METHOD)>::type>

using GetCapabilitiesExecutor = FUNC_TYPE(getCapabilities);

using ContinuousMoveExecutor = FUNC_TYPE(continuousMove);
using ContinuousFocusExecutor = FUNC_TYPE(continuousFocus);

using AbsoluteMoveExecutor = FUNC_TYPE(absoluteMove);
using ViewportMoveExecutor = FUNC_TYPE(viewportMove);

using RelativeMoveExecutor = FUNC_TYPE(relativeMove);
using RelativeFocusExecutor = FUNC_TYPE(relativeFocus);

using GetPositionExecutor = FUNC_TYPE(getPosition);
using GetLimitsExecutor = FUNC_TYPE(getLimits);
using GetFlipExecutor = FUNC_TYPE(getFlip);

using GetPresetsExecutor = FUNC_TYPE(getPresets);
using CreatePresetExecutor = FUNC_TYPE(createPreset);
using UpdatePresetExecutor = FUNC_TYPE(updatePreset);
using RemovePresetExecutor = FUNC_TYPE(removePreset);
using ActivatePresetExecutor = FUNC_TYPE(activatePreset);

using GetToursExecutor = FUNC_TYPE(getTours);
using CreateTourExecutor = FUNC_TYPE(createTour);
using RemoveTourExecutor = FUNC_TYPE(removeTour);
using ActivateTourExecutor = FUNC_TYPE(activateTour);

using GetActiveObjectExecutor = FUNC_TYPE(getActiveObject);
using GetHomeObjectExecutor = FUNC_TYPE(getHomeObject);
using UpdateHomeObjectExecutor = FUNC_TYPE(updateHomeObject);

using GetAuxiliaryTraitsExecutor = FUNC_TYPE(getAuxiliaryTraits);
using RunAuxiliaryCommandExecutor = FUNC_TYPE(runAuxiliaryCommand);

using GetDataExecutor = FUNC_TYPE(getData);

#undef FUNC_TYPE

class TestPtzController: public QnAbstractPtzController
{
    using base_type = QnAbstractPtzController;
public:
    TestPtzController();
    TestPtzController(const QnResourcePtr& resource);

    virtual Ptz::Capabilities getCapabilities(
        const nx::core::ptz::Options& options) const override;

    virtual bool continuousMove(
        const nx::core::ptz::Vector& speed,
        const nx::core::ptz::Options& options) override;

    virtual bool continuousFocus(
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool absoluteMove(
        Qn::PtzCoordinateSpace space,
        const nx::core::ptz::Vector& position,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool viewportMove(
        qreal aspectRatio,
        const QRectF& viewport,
        qreal speed,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeMove(
        const nx::core::ptz::Vector& direction,
        const nx::core::ptz::Options& options) override;

    virtual bool relativeFocus(qreal direction, const nx::core::ptz::Options& options) override;

    virtual bool getPosition(
        Qn::PtzCoordinateSpace space,
        nx::core::ptz::Vector* position,
        const nx::core::ptz::Options& options) const override;

    virtual bool getLimits(
        Qn::PtzCoordinateSpace space,
        QnPtzLimits* limits,
        const nx::core::ptz::Options& options) const override;

    virtual bool getFlip(
        Qt::Orientations* flip,
        const nx::core::ptz::Options& options) const override;

    virtual bool createPreset(const QnPtzPreset& preset) override;
    virtual bool updatePreset(const QnPtzPreset& preset) override;
    virtual bool removePreset(const QString& presetId) override;
    virtual bool activatePreset(const QString& presetId, qreal speed) override;
    virtual bool getPresets(QnPtzPresetList* presets) const override;

    virtual bool createTour(const QnPtzTour& tour) override;
    virtual bool removeTour(const QString& tourId) override;
    virtual bool activateTour(const QString& tourId) override;
    virtual bool getTours(QnPtzTourList* tours) const override;

    virtual bool getActiveObject(QnPtzObject* activeObject) const override;
    virtual bool updateHomeObject(const QnPtzObject& homeObject) override;
    virtual bool getHomeObject(QnPtzObject* homeObject) const override;

    virtual bool getAuxiliaryTraits(
        QnPtzAuxiliaryTraitList* auxiliaryTraits,
        const nx::core::ptz::Options& options) const override;
    virtual bool runAuxiliaryCommand(
        const QnPtzAuxiliaryTrait& trait,
        const QString& data,
        const nx::core::ptz::Options& options) override;

    virtual bool getData(
        Qn::PtzDataFields query,
        QnPtzData* data,
        const nx::core::ptz::Options& options) const override;

public:

#define MAKE_EXECUTOR_SETTER(EXECUTOR_TYPE, EXECUTOR_NAME)\
    void set##EXECUTOR_TYPE(EXECUTOR_TYPE executor)\
    {\
        m_##EXECUTOR_NAME##Executor = executor;\
    }

    MAKE_EXECUTOR_SETTER(GetCapabilitiesExecutor, getCapabilities)
    MAKE_EXECUTOR_SETTER(ContinuousMoveExecutor, continuousMove)
    MAKE_EXECUTOR_SETTER(ContinuousFocusExecutor, continuousFocus)

    MAKE_EXECUTOR_SETTER(AbsoluteMoveExecutor, absoluteMove)
    MAKE_EXECUTOR_SETTER(ViewportMoveExecutor, viewportMove)

    MAKE_EXECUTOR_SETTER(RelativeMoveExecutor, relativeMove)
    MAKE_EXECUTOR_SETTER(RelativeFocusExecutor, relativeFocus)

    MAKE_EXECUTOR_SETTER(GetPositionExecutor, getPosition)
    MAKE_EXECUTOR_SETTER(GetLimitsExecutor, getLimits)
    MAKE_EXECUTOR_SETTER(GetFlipExecutor, getFlip)

    MAKE_EXECUTOR_SETTER(CreatePresetExecutor, createPreset)
    MAKE_EXECUTOR_SETTER(UpdatePresetExecutor, updatePreset)
    MAKE_EXECUTOR_SETTER(RemovePresetExecutor, removePreset)
    MAKE_EXECUTOR_SETTER(ActivatePresetExecutor, activatePreset)
    MAKE_EXECUTOR_SETTER(GetPresetsExecutor, getPresets)

    MAKE_EXECUTOR_SETTER(CreateTourExecutor, createTour)
    MAKE_EXECUTOR_SETTER(RemoveTourExecutor, removeTour)
    MAKE_EXECUTOR_SETTER(ActivateTourExecutor, activateTour)
    MAKE_EXECUTOR_SETTER(GetToursExecutor, getTours)

    MAKE_EXECUTOR_SETTER(GetActiveObjectExecutor, getActiveObject)
    MAKE_EXECUTOR_SETTER(UpdateHomeObjectExecutor, updateHomeObject)
    MAKE_EXECUTOR_SETTER(GetHomeObjectExecutor, getHomeObject)

    MAKE_EXECUTOR_SETTER(GetAuxiliaryTraitsExecutor, getAuxiliaryTraits)
    MAKE_EXECUTOR_SETTER(RunAuxiliaryCommandExecutor, runAuxiliaryCommand)

    MAKE_EXECUTOR_SETTER(GetDataExecutor, getData)

#undef MAKE_EXECUTOR_SETTER

public:
    void setCapabilities(std::optional<Ptz::Capabilities> capabilities);
    void setPosition(std::optional<nx::core::ptz::Vector> position);
    void setLimits(std::optional<QnPtzLimits> limits);
    void setFlip(std::optional<Qt::Orientations> flip);
    void setPresets(std::optional<QnPtzPresetList> presets);
    void setTours(std::optional<QnPtzTourList> tours);
    void setActiveObject(std::optional<QnPtzObject> activeObject);
    void setHomeObject(std::optional<QnPtzObject> activeObject);
    void setAuxiliaryTraits(std::optional<QnPtzAuxiliaryTraitList> traits);
    void setData(std::optional<QnPtzData> data);

private:
    GetCapabilitiesExecutor m_getCapabilitiesExecutor;

    ContinuousMoveExecutor m_continuousMoveExecutor;
    ContinuousFocusExecutor m_continuousFocusExecutor;

    AbsoluteMoveExecutor m_absoluteMoveExecutor;
    ViewportMoveExecutor m_viewportMoveExecutor;

    RelativeMoveExecutor m_relativeMoveExecutor;
    RelativeFocusExecutor m_relativeFocusExecutor;

    GetPositionExecutor m_getPositionExecutor;
    GetLimitsExecutor m_getLimitsExecutor;
    GetFlipExecutor m_getFlipExecutor;

    GetPresetsExecutor m_getPresetsExecutor;
    CreatePresetExecutor m_createPresetExecutor;
    UpdatePresetExecutor m_updatePresetExecutor;
    RemovePresetExecutor m_removePresetExecutor;
    ActivatePresetExecutor m_activatePresetExecutor;

    GetToursExecutor m_getToursExecutor;
    CreateTourExecutor m_createTourExecutor;
    RemoveTourExecutor m_removeTourExecutor;
    ActivateTourExecutor m_activateTourExecutor;

    GetActiveObjectExecutor m_getActiveObjectExecutor;
    GetHomeObjectExecutor m_getHomeObjectExecutor;
    UpdateHomeObjectExecutor m_updateHomeObjectExecutor;

    GetAuxiliaryTraitsExecutor m_getAuxiliaryTraitsExecutor;
    RunAuxiliaryCommandExecutor m_runAuxiliaryCommandExecutor;

    GetDataExecutor m_getDataExecutor;

private:
    std::optional<Ptz::Capabilities> m_predefinedCapabilities;
    std::optional<nx::core::ptz::Vector> m_predefinedPosition;
    std::optional<QnPtzLimits> m_predefinedLimits;
    std::optional<Qt::Orientations> m_predefinedFlip;
    std::optional<QnPtzPresetList> m_predefinedPresets;
    std::optional<QnPtzTourList> m_predefinedTours;
    std::optional<QnPtzObject> m_predefinedActiveObject;
    std::optional<QnPtzObject> m_predefinedHomeObject;
    std::optional<QnPtzAuxiliaryTraitList> m_predefinedAuxiliaryTraits;
    std::optional<QnPtzData> m_predefinedData;
};

} // namespace test_support
} // namespace ptz
} // namespace core
} // namespace nx

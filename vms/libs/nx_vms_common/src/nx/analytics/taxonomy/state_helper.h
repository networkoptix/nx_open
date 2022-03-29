// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_state.h>
#include <core/resource/resource_fwd.h>

namespace nx::analytics::taxonomy {

template<typename EntityType>
struct ScopedEntity
{
    AbstractEngine* engine = nullptr;
    AbstractGroup* group = nullptr;
    EntityType* entityType = nullptr;

    ScopedEntity() = default;

    ScopedEntity(AbstractEngine* engine, AbstractGroup* group, EntityType* entityType):
        engine(engine),
        group(group),
        entityType(entityType)
    {
    }
};

template<typename EntityType>
struct GroupScope
{
    AbstractGroup* group = nullptr;
    std::vector<EntityType*> entities;
};

template<typename EntityType>
struct EngineScope
{
    AbstractEngine* engine = nullptr;
    std::vector<GroupScope<EntityType>> groups;
};

/**
 * Helper class for building different sets of analytics entities related to some device or a set
 * of devices. Unions, intersections and trees of supported and compatible analytics Object and
 * Event types can be build using this helper.
 *
 * Supported entities (Object or Event types) are the entities that are declared as supported by
 * Device Agents enabled for a certain device. In other words the corresponding Device Agents are
 * certainly able to produce Objects and Events of such types for the bound Device. When
 * calculating a set of supported entities only explicitly enabled by the User or compatible with
 * the Device and device-dependent analytics Engines are taken into account.
 *
 * Compatible entities are the supported entities plus a set of entities that
 * are potentially supported (declared in the Engine manifest) for the Device by the Engines
 * that are compatible with the Device but not enabled by the User and are not device-dependent.
 *
 * Entity tree is a structured set of entities grouped by Engine and Group.
 */
class NX_VMS_COMMON_API StateHelper
{
public:
    StateHelper(std::shared_ptr<AbstractState> state);

    /** @return Map of supported Event types indexed by the Event type id. */
    std::map<QString, AbstractEventType*> supportedEventTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of supported Event types indexed by the Event type id for a set of devices.
     *     Calculated as a union of maps of supported Event types of particular devices from the
     *     set.
     */
    std::map<QString, AbstractEventType*> supportedEventTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of supported Event types indexed by the Event type id for a set of devices.
     *     Calculated as an interseection of maps of supported Event types of particular devices
     *     from the set.
     */
    std::map<QString, AbstractEventType*> supportedEventTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of supported Event types for a set of devices. Calculated as an intersection of
     *     trees of supported Event types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractEventType>> supportedEventTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of supported Event types for a set of devices. Calculated as a union of trees
     *     of supported Event types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractEventType>> supportedEventTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of compatible Event types indexed by the Event type id. */
    std::map<QString, AbstractEventType*> compatibleEventTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of compatible Event types indexed by the Event type id for a set of devices.
     *     Calculated as a union of maps of compatible Event types of particular devices from the
     *     set.
     */
    std::map<QString, AbstractEventType*> compatibleEventTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of compatible Event types indexed by the Event type id for a set of devices.
     *     Calculated as an interseection of maps of compatible Event types of particular devices
     *     from the set.
     */
    std::map<QString, AbstractEventType*> compatibleEventTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of compatible Event types for a set of devices. Calculated as a union
     *     of trees of compatible Event types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractEventType>> compatibleEventTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of compatible Event types for a set of devices. Calculated as an intersection
     *     of trees of compatible Event types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractEventType>> compatibleEventTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractEventType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of supported Object types indexed by the Object type id. */
    std::map<QString, AbstractObjectType*> supportedObjectTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of supported Object types indexed by the Object type id for a set of devices.
     *     Calculated as a union of maps of supported Object types of particular devices from the
     *     set.
     */
    std::map<QString, AbstractObjectType*> supportedObjectTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of supported Object types indexed by the Object type id for a set of devices.
     *     Calculated as an interseection of maps of supported Object types of particular devices
     *     from the set.
     */
    std::map<QString, AbstractObjectType*> supportedObjectTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of supported Object types for a set of devices. Calculated as an intersection
     *     of trees of supported Object types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractObjectType>> supportedObjectTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of supported Object types for a set of devices. Calculated as a union of trees
     *     of supported Object types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractObjectType>> supportedObjectTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of compatible Object types indexed by the Object type id. */
    std::map<QString, AbstractObjectType*> compatibleObjectTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of compatible Object types indexed by the Object type id for a set of devices.
     *     Calculated as a union of maps of compatible Object types of particular devices from the
     *     set.
     */
    std::map<QString, AbstractObjectType*> compatibleObjectTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of compatible Object types indexed by the Object type id for a set of devices.
     *     Calculated as an interseection of maps of compatible Object types of particular devices
     *     from the set.
     */
    std::map<QString, AbstractObjectType*> compatibleObjectTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of compatible Object types for a set of devices. Calculated as a union
     *     of trees of compatible Object types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractObjectType>> compatibleObjectTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of compatible Object types for a set of devices. Calculated as an intersection
     *     of trees of compatible Object types of particular devices from the set.
     */
    std::vector<EngineScope<AbstractObjectType>> compatibleObjectTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<AbstractObjectType>>& additionalEntities = {}) const;

private:
    std::shared_ptr<AbstractState> m_state;
};

} // namespace nx::analytics::taxonomy

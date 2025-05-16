// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/analytics/taxonomy/abstract_state.h>

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
    std::map<QString, EventType*> supportedEventTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of supported Event types indexed by the Event type id for a set of devices.
     *     Calculated as a union of maps of supported Event types of particular devices from the
     *     set.
     */
    std::map<QString, EventType*> supportedEventTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of supported Event types indexed by the Event type id for a set of devices.
     *     Calculated as an intersection of maps of supported Event types of particular devices
     *     from the set.
     */
    std::map<QString, EventType*> supportedEventTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of supported Event types for a set of devices. Calculated as an intersection of
     *     trees of supported Event types of particular devices from the set.
     */
    std::vector<EngineScope<EventType>> supportedEventTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<EventType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of supported Event types for a set of devices. Calculated as a union of trees
     *     of supported Event types of particular devices from the set.
     */
    std::vector<EngineScope<EventType>> supportedEventTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<EventType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of compatible Event types indexed by the Event type id. */
    std::map<QString, EventType*> compatibleEventTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of compatible Event types indexed by the Event type id for a set of devices.
     *     Calculated as a union of maps of compatible Event types of particular devices from the
     *     set.
     */
    std::map<QString, EventType*> compatibleEventTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of compatible Event types indexed by the Event type id for a set of devices.
     *     Calculated as an intersection of maps of compatible Event types of particular devices
     *     from the set.
     */
    std::map<QString, EventType*> compatibleEventTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of compatible Event types for a set of devices. Calculated as a union
     *     of trees of compatible Event types of particular devices from the set.
     */
    std::vector<EngineScope<EventType>> compatibleEventTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<EventType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Event type tree should be built.
     * @param additionalEntities List of Event types that should be added to the output tree.
     * @return Tree of compatible Event types for a set of devices. Calculated as an intersection
     *     of trees of compatible Event types of particular devices from the set.
     */
    std::vector<EngineScope<EventType>> compatibleEventTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<EventType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of supported Object types indexed by the Object type id. */
    std::map<QString, ObjectType*> supportedObjectTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of supported Object types indexed by the Object type id for a set of devices.
     *     Calculated as a union of maps of supported Object types of particular devices from the
     *     set.
     */
    std::map<QString, ObjectType*> supportedObjectTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of supported Object types indexed by the Object type id for a set of devices.
     *     Calculated as an intersection of maps of supported Object types of particular devices
     *     from the set.
     */
    std::map<QString, ObjectType*> supportedObjectTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of supported Object types for a set of devices. Calculated as an intersection
     *     of trees of supported Object types of particular devices from the set.
     */
    std::vector<EngineScope<ObjectType>> supportedObjectTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<ObjectType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of supported Object types for a set of devices. Calculated as a union of trees
     *     of supported Object types of particular devices from the set.
     */
    std::vector<EngineScope<ObjectType>> supportedObjectTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<ObjectType>>& additionalEntities = {}) const;

    //----------------------------------------------------------------------------------------------

    /** @return Map of compatible Object types indexed by the Object type id. */
    std::map<QString, ObjectType*> compatibleObjectTypes(
        const QnVirtualCameraResourcePtr& device) const;

    /**
     * @return Map of compatible Object types indexed by the Object type id for a set of devices.
     *     Calculated as a union of maps of compatible Object types of particular devices from the
     *     set.
     */
    std::map<QString, ObjectType*> compatibleObjectTypeUnion(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @return Map of compatible Object types indexed by the Object type id for a set of devices.
     *     Calculated as an intersection of maps of compatible Object types of particular devices
     *     from the set.
     */
    std::map<QString, ObjectType*> compatibleObjectTypeIntersection(
        const QnVirtualCameraResourceList& device) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of compatible Object types for a set of devices. Calculated as a union
     *     of trees of compatible Object types of particular devices from the set.
     */
    std::vector<EngineScope<ObjectType>> compatibleObjectTypeTreeUnion(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<ObjectType>>& additionalEntities = {}) const;

    /**
     * @param devices List of devices for which the Object type tree should be built.
     * @param additionalEntities List of Object types that should be added to the output tree.
     * @return Tree of compatible Object types for a set of devices. Calculated as an intersection
     *     of trees of compatible Object types of particular devices from the set.
     */
    std::vector<EngineScope<ObjectType>> compatibleObjectTypeTreeIntersection(
        const QnVirtualCameraResourceList& devices,
        const std::vector<ScopedEntity<ObjectType>>& additionalEntities = {}) const;

private:
    std::shared_ptr<AbstractState> m_state;
};

} // namespace nx::analytics::taxonomy

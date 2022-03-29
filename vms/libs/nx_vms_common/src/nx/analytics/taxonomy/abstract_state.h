// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QString>

#include <nx/vms/api/analytics/descriptors.h>
#include <nx/analytics/taxonomy/abstract_plugin.h>
#include <nx/analytics/taxonomy/abstract_engine.h>
#include <nx/analytics/taxonomy/abstract_group.h>
#include <nx/analytics/taxonomy/abstract_object_type.h>
#include <nx/analytics/taxonomy/abstract_event_type.h>
#include <nx/analytics/taxonomy/abstract_enum_type.h>
#include <nx/analytics/taxonomy/abstract_color_type.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractState: public QObject
{
    Q_OBJECT

    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractObjectType*> rootObjectTypes
        READ rootObjectTypes CONSTANT)

    Q_PROPERTY(std::vector<nx::analytics::taxonomy::AbstractEngine*> engines
        READ engines CONSTANT)

public:
    virtual ~AbstractState() {}

    virtual std::vector<AbstractPlugin*> plugins() const = 0;

    virtual std::vector<AbstractEngine*> engines() const = 0;

    virtual std::vector<AbstractGroup*> groups() const = 0;

    virtual std::vector<AbstractObjectType*> objectTypes() const = 0;

    virtual std::vector<AbstractEventType*> eventTypes() const = 0;

    virtual std::vector<AbstractEnumType*> enumTypes() const = 0;

    virtual std::vector<AbstractColorType*> colorTypes() const = 0;

    virtual std::vector<AbstractObjectType*> rootObjectTypes() const = 0;

    virtual std::vector<AbstractEventType*> rootEventTypes() const = 0;

    virtual AbstractObjectType* objectTypeById(const QString& objectTypeId) const = 0;

    virtual AbstractEventType* eventTypeById(const QString& eventTypeId) const = 0;

    virtual AbstractPlugin* pluginById(const QString& pluginId) const = 0;

    virtual AbstractEngine* engineById(const QString& engineId) const = 0;

    virtual AbstractGroup* groupById(const QString& groupId) const = 0;

    virtual AbstractEnumType* enumTypeById(const QString& enumTypeId) const = 0;

    virtual AbstractColorType* colorTypeById(const QString& colorTypeId) const = 0;

    virtual nx::vms::api::analytics::Descriptors serialize() const = 0;
};

} // namespace nx::analytics::taxonomy

Q_DECLARE_METATYPE(std::vector<nx::analytics::taxonomy::AbstractObjectType*>)
Q_DECLARE_METATYPE(std::vector<nx::analytics::taxonomy::AbstractEngine*>)

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

#include <QtCore/QString>

#include <nx/utils/impl_ptr.h>

#include "attribute.h"

namespace nx::analytics::taxonomy { class AbstractObjectType; }

namespace nx::vms::client::core::analytics::taxonomy {

class AbstractStateViewFilter;

/**
 * Represents a taxonomy entity (Object Type) with attributes. Object Types of the State View are
 * not necessary in "one-to-one" relationship with the corresponding basic taxonomy Object Types.
 * State View Object Type can be built from multiple base Object Types (e.g. some Object Type and
 * its hidden derived descendants), and potentially can be a virtual entity that has no direct
 * match in the analytics taxonomy.
 */
class NX_VMS_CLIENT_CORE_API ObjectType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> typeIds READ typeIds CONSTANT)
    Q_PROPERTY(QString mainTypeId READ mainTypeId CONSTANT)
    Q_PROPERTY(std::vector<QString> fullSubtreeTypeIds READ fullSubtreeTypeIds CONSTANT)
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString iconSource READ icon CONSTANT)
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::Attribute*> attributes
        READ attributes CONSTANT)
    Q_PROPERTY(nx::vms::client::core::analytics::taxonomy::ObjectType* baseObjectType
        READ baseObjectType CONSTANT)
    Q_PROPERTY(std::vector<nx::vms::client::core::analytics::taxonomy::ObjectType*>
        derivedObjectTypes
        READ derivedObjectTypes CONSTANT)

public:
    ObjectType(const nx::analytics::taxonomy::AbstractObjectType* mainObjectType,
        QObject* parent = nullptr);

    virtual ~ObjectType() override;

    /**
     * Ids of Object or Event types from which the node is built, including hidden descendants.
     */
    std::vector<QString> typeIds() const;

    /**
     * Id of main Object or Event type from which the node is built.
     */
    QString mainTypeId() const;

    /**
     * Ids of Object or Event types from which the node is built and ids of all its explicit and
     * hidden descendants that passes the filter which was used to build the state view.
     */
    std::vector<QString> fullSubtreeTypeIds() const;

    QString id() const;

    QString name() const;

    QString icon() const;

    std::vector<Attribute*> attributes() const;

    ObjectType* baseObjectType() const;

    void setBaseObjectType(ObjectType* objectType);

    std::vector<ObjectType*> derivedObjectTypes() const;

    void addObjectType(const nx::analytics::taxonomy::AbstractObjectType* objectType);

    void addDerivedObjectType(ObjectType* objectType);

    void setFilter(const AbstractStateViewFilter* filter);

    void resolveAttributes();

    static QString makeId(const QStringList& analyticsObjectTypeIds);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core::analytics::taxonomy

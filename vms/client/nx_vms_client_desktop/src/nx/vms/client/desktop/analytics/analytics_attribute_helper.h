// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVector>
#include <QtCore/QVariantList>

#include <analytics/common/object_metadata.h>
#include <nx/utils/impl_ptr.h>

namespace nx::analytics::taxonomy {
class AbstractAttribute;
class AbstractStateWatcher;
} // namespace nx::analytics::taxonomy

namespace nx::vms::client::desktop::analytics {

struct Attribute
{
    QString id;
    QStringList values;
    QString displayedName;
    QStringList displayedValues;

    Q_GADGET
    Q_PROPERTY(QString id MEMBER id CONSTANT)
    Q_PROPERTY(QStringList values MEMBER values CONSTANT)
    Q_PROPERTY(QString displayedName MEMBER displayedName CONSTANT)
    Q_PROPERTY(QStringList displayedValues MEMBER displayedValues CONSTANT)

public:
    bool operator==(const Attribute& other) const
    {
        return id == other.id
            && displayedName == other.displayedName
            && values == other.values
            && displayedValues == other.displayedValues;
    };

    bool operator!=(const Attribute& other) const
    {
        return !(*this == other);
    }
};

using AttributeList = QList<Attribute>;

class AttributeHelper
{
public:
    explicit AttributeHelper(
        nx::analytics::taxonomy::AbstractStateWatcher* taxonomyStateWatcher);
    virtual ~AttributeHelper();

    using AttributePath = QVector<nx::analytics::taxonomy::AbstractAttribute*>;
    AttributePath attributePath(const QString& objectTypeId, const QString& attributeName) const;

    AttributeList preprocessAttributes(const QString& objectTypeId,
        const nx::common::metadata::Attributes& sourceAttributes) const;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::analytics

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/utils/uuid.h>

namespace nx::vms::common::test {

/**
 * A helper class to identify uuid-identified entities by name.
 * Use `ensureId` to create a new uuid for a given name or get an existing one.
 */
class NX_VMS_COMMON_TEST_SUPPORT_API NameToId
{
public:
    explicit NameToId() = default;
    NameToId(const NameToId&) = default;
    ~NameToId() = default;

    void clear();
    nx::Uuid ensureId(const QString& name) const;

    QString name(const nx::Uuid& id) const;
    nx::Uuid id(const QString& name) const;

    void setId(const QString& name, const nx::Uuid& id);

    using Names = std::set<QString>;
    using Ids = QSet<nx::Uuid>;

    Ids ensureIds(const Names& names) const;
    Ids ids(const Names& names) const;
    Names names(const Ids& ids) const;

private:
    mutable QHash<QString, nx::Uuid> m_nameToId;
    mutable QHash<nx::Uuid, QString> m_idToName;
};

} // namespace nx::vms::common::test

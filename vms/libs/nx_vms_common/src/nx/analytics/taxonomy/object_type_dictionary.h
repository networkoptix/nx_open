// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <analytics/db/abstract_object_type_dictionary.h>

namespace nx::analytics::taxonomy {

class AbstractStateWatcher;

class NX_VMS_COMMON_API ObjectTypeDictionary:
    public QObject,
    public nx::analytics::db::AbstractObjectTypeDictionary
{
    Q_OBJECT
public:
    explicit ObjectTypeDictionary(AbstractStateWatcher* taxonomyWatcher, QObject* parent = nullptr);
    virtual std::optional<QString> idToName(const QString& id) const override;

private:
    const QPointer<AbstractStateWatcher> m_taxonomyWatcher;
};

} // namespace nx::analytics::taxonomy

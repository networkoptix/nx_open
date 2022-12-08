// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaClassInfo>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

/**
 * Base class for storing actual rule values for event filters and action builders.
 */
class NX_VMS_RULES_API Field: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static constexpr auto kMetatype = "metatype";

public:
    Field();

    QString metatype() const;

    void connectSignals();

    QMap<QString, QJsonValue> serializedProperties() const;

    /**
     * Set field properties.
     * @return Whether all the properties is set successfully.
     */
    bool setProperties(const QVariantMap& properties);

    bool event(QEvent* ev) override;

signals:
    void stateChanged();

private:
    void notifyParent();

private:
    bool m_connected = false;
    bool m_updateInProgress = false;
};

template <class T>
QString fieldMetatype()
{
    const auto& meta = T::staticMetaObject;
    int idx = meta.indexOfClassInfo(Field::kMetatype);

    return (idx < 0) ? QString() : meta.classInfo(idx).value();
}

} // namespace nx::vms::rules

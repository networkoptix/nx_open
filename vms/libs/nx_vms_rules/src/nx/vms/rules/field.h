// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QObject>

namespace nx::vms::rules {

/**
 * Base class for storing actual rule values for event filters and action builders.
 */
class NX_VMS_RULES_API Field: public QObject
{
    Q_OBJECT

public:
    Field();

    virtual QString metatype() const;

    void connectSignals();

    QMap<QString, QJsonValue> serializedProperties() const;

    bool event(QEvent* ev) override;

signals:
    void stateChanged();

private:
    void notifyParent();

private:
    bool m_connected = false;
    bool m_updateInProgress = false;

};

} // namespace nx::vms::rules

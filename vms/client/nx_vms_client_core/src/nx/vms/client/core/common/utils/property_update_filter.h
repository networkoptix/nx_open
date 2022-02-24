// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtQml/QQmlPropertyValueSource>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {

/**
 * QML property value source that limits target property update frequency to a specified interval.
 *
 * Sample usage:
 *
 * RightPanelModel
 * {
 *     id: rightPanelModel
 *
 *     PropertyUpdateFilter on textFilter
 *     {
 *         source: searchField.text
 *         enabled: rightPanelModel.supportsTextFilter
 *         minimumIntervalMs: 250
 *     }
 * }
 *
 * SearchField
 * {
 *     id: searchField
 * }
 */

class PropertyUpdateFilter:
    public QObject,
    public QQmlPropertyValueSource
{
    Q_OBJECT
    Q_PROPERTY(QVariant source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int minimumIntervalMs READ minimumIntervalMs WRITE setMinimumIntervalMs
        NOTIFY minimumIntervalChanged)
    Q_PROPERTY(UpdateFlags updateFlags READ updateFlags WRITE setUpdateFlags
        NOTIFY updateFlagsChanged)

    // This statement requires specifying Qt includes dir for the MOC call for this header.
    Q_INTERFACES(QQmlPropertyValueSource)

public:
    explicit PropertyUpdateFilter(QObject* parent = nullptr);
    virtual ~PropertyUpdateFilter() override;

    virtual void setTarget(const QQmlProperty& value) override;

    QVariant source() const;
    void setSource(const QVariant& value);

    bool enabled() const; //< Default true.
    void setEnabled(bool value);

    int minimumIntervalMs() const; //< Default 200ms.
    void setMinimumIntervalMs(int value);

    enum UpdateFlag
    {
        None = 0,
        ImmediateFirstUpdate = 0x1,
        UpdateOnlyWhenIdle = 0x2
    };
    Q_DECLARE_FLAGS(UpdateFlags, UpdateFlag)

    UpdateFlags updateFlags() const; //< Default UpdateOnlyWhenIdle.
    void setUpdateFlags(UpdateFlags value);

    static void registerQmlType();

signals:
    void sourceChanged();
    void enabledChanged();
    void minimumIntervalChanged();
    void updateFlagsChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PropertyUpdateFilter::UpdateFlags)

} // namespace nx::vms::client::core

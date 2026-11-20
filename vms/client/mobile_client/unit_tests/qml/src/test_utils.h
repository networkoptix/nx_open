// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile::test {

class TestUtils: public QObject
{
    Q_OBJECT

public:
    /** Finds an object by QML id in the type of `root`, its base types and nested components. */
    Q_INVOKABLE QObject* findById(QObject* root, const QString& id) const;

    /**
     * Lists the members of the mock which the class does not have, nested objects included.
     * Skips reset() of the mock itself and `<function>Called` signals.
     * @param typeName Full class name, known to the meta type system.
     */
    Q_INVOKABLE QStringList mockMismatches(QObject* mock, const QString& typeName) const;

    static void registerQmlType();
};

} // namespace nx::vms::client::mobile::test

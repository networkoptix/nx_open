// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile::test {

/** Stands in for a QML screen which reports its result with the gotResult() signal. */
class FakeResultScreen: public QObject
{
    Q_OBJECT

signals:
    void gotResult(const QString& value);
};

} // namespace nx::vms::client::mobile::test

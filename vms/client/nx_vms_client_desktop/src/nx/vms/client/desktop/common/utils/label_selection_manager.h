// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

class QCoreApplication;

namespace nx::vms::client::desktop {

/**
 * A class to customize QLabel selection behavior in accordance with Nx style.
 * Should be simply initialized once a QCoreApplication instance is available.
 */
class LabelSelectionManager final: public QObject
{
    Q_OBJECT

    explicit LabelSelectionManager(QCoreApplication* application);
    virtual ~LabelSelectionManager() override;

public:
    static bool init(QCoreApplication* application);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop

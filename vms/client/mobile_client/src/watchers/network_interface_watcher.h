// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QObject>

namespace nx::vms::client::mobile {

class NetworkInterfaceWatcher: public QObject
{
    Q_OBJECT

public:
    NetworkInterfaceWatcher();

signals:
    void interfacesChanged();
};

} // namespace nx::vms::client::mobile

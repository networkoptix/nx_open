// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QTimer>

#include <nx/utils/impl_ptr.h>

class QnLicensePool;

namespace nx::vms::client::desktop {

class LicenseHealthWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    LicenseHealthWatcher(QnLicensePool* licensePool, QObject* parent = nullptr);

private slots:
    void at_timer();

private:
    QTimer m_timer;
    QnLicensePool* m_licensePool;
};

} // namespace nx::vms::client::desktop

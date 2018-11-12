#pragma once

#include <QtCore/QString>

#if defined(Q_OS_MACX)

class QnBundleHelpers
{
public:
    static bool isHiDpiSupported();

    static bool isInHiDpiMode(const QString& path);

private:
    QnBundleHelpers() = default;
    ~QnBundleHelpers() = default;
};

#endif // defined(Q_OS_MACX)

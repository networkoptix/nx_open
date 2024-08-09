// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#if defined(Q_OS_MACOS)

class QnBundleHelpers
{
public:
    static bool isHiDpiSupported();

    static bool isInHiDpiMode(const QString& path);

private:
    QnBundleHelpers() = default;
    ~QnBundleHelpers() = default;
};

#endif // defined(Q_OS_MACOS)

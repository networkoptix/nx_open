
#pragma once

class QnBundleHelpers
{
public:
    static bool isHiDpiSupported();

    static bool isInHiDpiMode(const QString& path);

private:
    QnBundleHelpers() = default;
    ~QnBundleHelpers() = default;
};

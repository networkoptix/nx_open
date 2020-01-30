#pragma once

#include <vector>

#include <QByteArray>
#include <QFile>
#include <QString>

#include <nx/utils/url.h>

namespace nx::vms::utils {

struct ResourceParamsData
{
    QString location;
    QByteArray value;

    static ResourceParamsData load(const nx::utils::Url& url);
    static ResourceParamsData load(QFile&& file);

    static ResourceParamsData getWithGreaterVersion(
        const std::vector<ResourceParamsData>& datas);
};

} // namespace nx::vms::utils

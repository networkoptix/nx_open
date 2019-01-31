#pragma once

#include <core/resource/resource_fwd.h>

struct QnResourceCompareHelper
{
    static QnResourcePtr getResource(const QModelIndex& index);
    static bool resourceLessThan(const QModelIndex& left, const QModelIndex& right);
};

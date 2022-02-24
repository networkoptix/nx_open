// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QModelIndex>


class QCollator;

struct QnResourceCompareHelper
{
    static bool resourceLessThan(const QModelIndex& left, const QModelIndex& right,
        const QCollator& collator);
};

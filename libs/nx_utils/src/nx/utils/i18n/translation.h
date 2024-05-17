// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace nx::i18n {

struct Translation
{
    /** Locale code, e.g. "zh_CN". */
    QString localeCode;

    /** Paths to .qm files. */
    QStringList filePaths;

    QString toString() const;
};

} // namespace nx::i18n

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include "conversion_wrapper.h"

class QnJsonContext;
class QJsonValue;

namespace nx::utils {

struct Void
{
    inline Void getId() const { return Void(); }
    inline QString toString() const { return QString(); }
    bool operator==(const Void&) const { return false; }
};

inline void serialize(QnJsonContext*, const Void&, QJsonValue*) {}
inline bool deserialize(QnJsonContext*, QnConversionWrapper<QJsonValue>, Void*) { return true; }

} // namespace nx::utils

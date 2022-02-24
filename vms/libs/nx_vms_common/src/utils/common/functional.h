// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QList>

namespace Qn {

using Notifier = std::function<void(void)>;
using NotifierList = QList<Notifier>;

}

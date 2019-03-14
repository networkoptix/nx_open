#pragma once

namespace Qn {

using Notifier = std::function<void(void)>;
using NotifierList = QList<Notifier>;

}

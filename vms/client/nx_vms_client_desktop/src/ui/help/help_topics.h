#pragma once
#if !defined(Q_MOC_RUN)

namespace Qn {

enum HelpTopic {
    Empty_Help = 0,
    Forced_Empty_Help,

#define QN_HELP_TOPIC(ID, URL) ID,
#include "help_topics.i"
#undef QN_HELP_TOPIC

    HelpTopicCount
};

} // namespace Qn

#endif // !defined(Q_MOC_RUN)

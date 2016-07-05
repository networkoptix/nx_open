#ifndef QN_HELP_TOPICS_H
#define QN_HELP_TOPICS_H

namespace Qn {

    enum HelpTopic {
        Empty_Help = 0,
        Forced_Empty_Help,

#define QN_HELP_TOPIC(ID, URL) ID,
#include "help_topics.i"

        HelpTopicCount
    };

} // namespace Qn

#endif // QN_HELP_TOPICS_H

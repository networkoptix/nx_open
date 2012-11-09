#ifndef QN_HELP_TOPICS_H
#define QN_HELP_TOPICS_H

namespace Qn {

    enum HelpTopic {
#define QN_HELP_TOPIC(ID, URL) ID,
#include "help_topics.i"

        Empty_Help = -1
    };

} // namespace Qn

#endif // QN_HELP_TOPICS_H

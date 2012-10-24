#include "context_help_queryable.h"

#include <QtCore/QObject>

#include <utils/common/warnings.h>

#define HelpTopicId _id("_qn_contextHelpId")

void setHelpTopic(QObject *object, int helpTopic) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    object->setProperty(Qn::HelpTopicId, helpTopic);
}

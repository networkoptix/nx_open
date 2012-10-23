#include "context_help_queryable.h"

#include <QtCore/QObject>

#include <utils/common/warnings.h>

void setHelpTopicId(QObject *object, int helpTopic) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    object->setProperty(Qn::HelpTopicId, helpTopic);
}

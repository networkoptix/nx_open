#include "context_help_queryable.h"

#include <QtCore/QObject>

#include <utils/common/warnings.h>

void setHelpTopicId(QWidget *object, Qn::HelpTopic helpTopic) {
    if(!object) {
        qnNullWarning(object);
        return;
    }

    object->setProperty(Qn::HelpTopicId, static_cast<int>(helpTopic));
}

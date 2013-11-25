#include "customization.h"

#include <utils/common/json.h>
#include <utils/common/warnings.h>

namespace {
    const char *customizationPropertyName = "_qn_customization";
}

QnCustomization::QnCustomization() {
    return;
}

QnCustomization::QnCustomization(const QString &fileName) {
    QFile file(fileName);
    if(!file.open(QFile::ReadOnly))
        return;

    if(!QJson::deserialize(file.readAll(), &m_data))
        qnWarning("Customization file '%1' has invalid format.", fileName);
}


#include "customization.h"

#include <utils/serialization/json_functions.h>
#include <utils/common/warnings.h>

namespace {
    QJsonObject mergeObjects(const QJsonObject &l, const QJsonObject &r) {
        if(r.isEmpty())
            return l;

        QJsonObject result = l;

        for(auto rPos = r.begin(); rPos != r.end(); rPos++) {
            const QString &key = rPos.key();

            auto lPos = result.find(key);
            if(lPos == result.end()) {
                result.insert(key, rPos.value());
                continue;
            }

            if(rPos.value().type() == QJsonValue::Object && lPos.value().type() == QJsonValue::Object) {
                *lPos = mergeObjects(lPos.value().toObject(), rPos.value().toObject());
            } else {
                *lPos = *rPos;
            }
        }

        return result;
    }
} // anonymous namespace

QnCustomization::QnCustomization(const QString &fileName) {
    if(fileName.isEmpty())
        return;

    QFile file(fileName);
    if(!file.open(QFile::ReadOnly))
        return;

    if(!QJson::deserialize(file.readAll(), &m_data))
        qnWarning("Customization file '%1' has invalid format.", fileName);
}

void QnCustomization::add(const QnCustomization &other) {
    m_data = mergeObjects(m_data, other.m_data);
}

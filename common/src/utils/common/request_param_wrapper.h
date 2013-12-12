#ifndef REQUEST_PARAM_WRAPPER_H
#define REQUEST_PARAM_WRAPPER_H

#include <utils/common/request_param.h>

class QnRequestParamWrapper {
public:
    QnRequestParamWrapper(const QnRequestParamList &list) {
        foreach (const QnRequestHeader &header, list)
            m_params[header.first] = header.second;
    }

    bool contains(const QString &key) const {
        return m_params.contains(key);
    }

    QString value(const QString &key) const {
        return m_params.value(key);
    }

    QString value(const QString &key, const QString &defaultValue) const {
        if (!m_params.contains(key))
            return defaultValue;
        return m_params.value(key);
    }

private:
    QHash<QString, QString> m_params;
};

#endif // REQUEST_PARAM_WRAPPER_H

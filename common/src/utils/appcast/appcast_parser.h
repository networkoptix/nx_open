#ifndef APPCAST_PARSER_H_
#define APPCAST_PARSER_H_

#include "update_info.h"

class QnAppCastParser {
public:
    void setPlatform(const QString& platform);
    void parse(const QByteArray& data);

    QnUpdateInfoItems items() const;
    QnUpdateInfoItems newItems(const QnVersion&) const;

private:
    QString m_platform;
    QString currentTag;

    QnUpdateInfoItems m_items;
};

#endif // APPCAST_PARSER_H_
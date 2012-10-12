#ifndef QN_APPCAST_PARSER_H
#define QN_APPCAST_PARSER_H

#include "update_info.h"

class QnAppCastParser {
public:
    void setPlatform(const QString& platform);
    void parse(const QByteArray& data);

    QnUpdateInfoItemList items() const;
    QnUpdateInfoItemList newItems(const QnVersion &) const;

private:
    QString m_platform;
    QString currentTag;

    QnUpdateInfoItemList m_items;
};

#endif // QN_APPCAST_PARSER_H
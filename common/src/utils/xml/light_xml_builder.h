#ifndef LIGHT_XML_BUILDER_H
#define LIGHT_XML_BUILDER_H

#include <QtCore/QByteArray>
#include <QtCore/QList>

class QnLightXmlBuilder
{
public:
    QnLightXmlBuilder();

    void beginSection(QByteArray section);
    void endSection();

    void writeValue(QByteArray name, QByteArray value);
    void writeValue(QByteArray name, QString value);
    void writeValue(QByteArray name, qint64 value);
    void writeValue(QByteArray name, quint64 value);

    QByteArray body();
    bool isValid();
private:
    QByteArray m_body;
    QList<QByteArray> m_sections;
    bool m_valid;

};

#endif // LIGHT_XML_BUILDER_H

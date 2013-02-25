#include "light_xml_builder.h"

#include <QtCore/QString>

QnLightXmlBuilder::QnLightXmlBuilder():
    m_valid(true)
{
}

void QnLightXmlBuilder::beginSection(QByteArray section) {
    m_sections << section;
    m_body.append("<" + section +">\n");
}

void QnLightXmlBuilder::endSection() {
    if (m_sections.isEmpty())
        m_valid = false;
    if (!m_valid)
        return;

    QByteArray section = m_sections.takeLast();
    m_body.append("</" + section + ">\n");
}

void QnLightXmlBuilder::writeValue(QByteArray name, QByteArray value) {
    m_body.append("<" + name+ ">" + value + "<" + name + ">\n");
}

void QnLightXmlBuilder::writeValue(QByteArray name, QString value) {
    m_body.append("<" + name+ ">" + value.toLatin1() + "<" + name + ">\n");
}

void QnLightXmlBuilder::writeValue(QByteArray name, qint64 value) {
    m_body.append("<" + name+ ">" + QString::number(value).toLatin1() + "<" + name + ">\n");
}

void QnLightXmlBuilder::writeValue(QByteArray name, quint64 value) {
    m_body.append("<" + name+ ">" + QString::number(value).toLatin1() + "<" + name + ">\n");
}

QByteArray QnLightXmlBuilder::body() {
    return "<root>\n" + m_body + "</root>\n";
}

bool QnLightXmlBuilder::isValid() {
    return m_valid && m_sections.isEmpty();
}

#include "proxystyle.h"

#include <QtGui/QApplication>
#include <QtGui/QStyleFactory>

ProxyStyle::ProxyStyle(QStyle *baseStyle)
    : m_style(baseStyle)
{
    setBaseStyle(baseStyle ? baseStyle : QApplication::style());
}

ProxyStyle::ProxyStyle(const QString &baseStyle, QObject *parent)
    : m_style(0)
{
    QStyle *style = QStyleFactory::create(baseStyle);
    if (!style)
        style = QApplication::style();
    style->setParent(parent);
    setBaseStyle(style);
}

QStyle *ProxyStyle::baseStyle() const
{
    if (!m_style)
        const_cast<ProxyStyle *>(this)->setBaseStyle(QApplication::style());
    return m_style;
}

void ProxyStyle::setBaseStyle(QStyle *style)
{
    m_style = style;
    setParent(m_style);
}

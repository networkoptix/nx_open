#include "proxystyle.h"

#include <QtGui/QApplication>
#include <QtGui/QStyleFactory>

#include <utils/common/warnings.h>

/* This object takes ownership of application styles. 
 * 
 * Proxy style cannot take ownership of application style because application
 * style will be destroyed once the proxy style is destroyed.
 * 
 * In the same time, ownership of the application style must be transferred
 * away from the application instance, since setting the global style to
 * proxy style will delete the current global style unless it is not
 * owned by the application instance.
 *
 * Hence this little hack.
 */
Q_GLOBAL_STATIC(QObject, styleStorage);

ProxyStyle::ProxyStyle(QStyle *baseStyle)
    : m_style(0)
{
    setBaseStyle(baseStyle);
}

ProxyStyle::ProxyStyle(const QString &baseStyle, QObject *parent)
    : m_style(0)
{
    setParent(parent);
    setBaseStyle(QStyleFactory::create(baseStyle));
}

QStyle *ProxyStyle::baseStyle() const
{
    if (!m_style)
        const_cast<ProxyStyle *>(this)->setBaseStyle(0); /* Will reinit base style with QApplication's style. */
    return m_style;
}

void ProxyStyle::setBaseStyle(QStyle *style)
{
    if(style == this) {
        qnWarning("Setting base of a proxy style to itself will lead to infinite recursion, falling back to windows style.");
        style = QStyleFactory::create(QLatin1String("windows"));
    }

    if(style) {
        m_style = style;
        m_style->setParent(this);
    } else {
        m_style = QApplication::style();
        m_style->setParent(styleStorage());
    }
}

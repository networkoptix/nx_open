#include "proxy_style.h"

#include <QtGui/QApplication>
#include <QtGui/QStyleFactory>

#include <utils/common/warnings.h>

/** 
 * This class takes ownership of application styles. 
 * 
 * Proxy style cannot take ownership of application style because then application
 * style will be destroyed once the proxy style is destroyed.
 * 
 * At the same time, ownership of the application style must be transferred
 * away from the application instance, since setting the global style to
 * proxy style will delete the current global style if it is owned 
 * by the application instance.
 *
 * Hence this little hack.
 */
class QnStyleStorage: public QObject {
public:
    QnStyleStorage(QObject *parent = NULL): QObject(parent) {}

    virtual ~QnStyleStorage() {
        if(!qApp)
            return;

        /* If application instance is still there, then we should transfer style
         * ownership back so that it gets destroyed properly. */
        foreach(QObject *child, children()) {
            if(child == qApp->style()) {
                child->setParent(qApp);
                break;
            }
        }
    }
};

Q_GLOBAL_STATIC(QnStyleStorage, styleStorage);

QnProxyStyle::QnProxyStyle(QStyle *baseStyle, QObject *parent)
{
    setParent(parent);
    setBaseStyle(baseStyle);
}

QnProxyStyle::QnProxyStyle(const QString &baseStyle, QObject *parent)
{
    setParent(parent);
    setBaseStyle(QStyleFactory::create(baseStyle));
}

QStyle *QnProxyStyle::baseStyle() const
{
    if (!m_style)
        const_cast<QnProxyStyle *>(this)->setBaseStyle(0); /* Will reinit base style with QApplication's style. */
    return m_style.data();
}

void QnProxyStyle::setBaseStyle(QStyle *style)
{
    if(style == this) {
        qnWarning("Setting base of a proxy style to itself will lead to infinite recursion, falling back to windows style.");
        style = QStyleFactory::create(QLatin1String("windows"));
    }

    if(style) {
        m_style = style;
    } else {
        m_style = QApplication::style();
    }
    m_style.data()->setParent(styleStorage());
}

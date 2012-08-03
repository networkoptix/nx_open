#include "context_help.h"
#include <QTranslator>
#include <QStringList>
#include <QFile>

Q_GLOBAL_STATIC(QnContextHelp, getInstance)

QnContextHelp::QnContextHelp(): 
    m_translator(0),
    m_currentId(ContextId_Invalid),
    m_autoShowNeeded(true)
{
    m_settings.beginGroup(QLatin1String("helpContext"));
    deserializeShownContext();
}

QnContextHelp* QnContextHelp::instance()
{
    return getInstance();
}

QnContextHelp::~QnContextHelp()
{
    delete m_translator;
}

void QnContextHelp::deserializeShownContext()
{
    QVariant value = m_settings.value(QLatin1String("autoShowNeeded"));
    if(value.isValid())
        m_autoShowNeeded = value.toBool();
}

void QnContextHelp::serializeShownContext()
{
    m_settings.setValue(QLatin1String("autoShowNeeded"), m_autoShowNeeded);
}

bool QnContextHelp::isAutoShowNeeded() const
{
    return m_autoShowNeeded;
}

void QnContextHelp::setHelpContext(ContextId id)
{
    if(m_currentId == id)
        return;

    m_currentId = id;
    emit helpContextChanged(id);
}

void QnContextHelp::setAutoShowNeeded(bool value)
{
    m_autoShowNeeded = value;
    serializeShownContext();
}

QnContextHelp::ContextId QnContextHelp::currentId() const {
    return m_currentId;
}

QString QnContextHelp::text(ContextId id) const {
    switch(id)
    {
    case ContextId_Scene:
        return tr("ContextId_Scene");
        break;
    case ContextId_MotionGrid:
        return tr("ContextId_MotionGrid");
        break;
    case ContextId_Slider:
        return tr("ContextId_Slider");
        break;
    case ContextId_Tree:
        return tr("ContextId_Tree");
        break;
    default:
        qWarning() << "Unknown help context" << (int) m_currentId;
        return QString();
    }
}

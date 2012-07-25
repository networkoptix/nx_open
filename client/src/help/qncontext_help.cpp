#include "qncontext_help.h"
#include <QTranslator>
#include <QStringList>
#include <QFile>

Q_GLOBAL_STATIC(QnContextHelp, getInstance)

QnContextHelp::QnContextHelp(): 
    m_translator(0),
    m_currentId(ContextId_Invalid),
    m_autoShowNeeded(true)
{
    installHelpContext(QLatin1String("en"));
    
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

void QnContextHelp::installHelpContext(const QString& lang)
{
    QString trFileName = QLatin1String(":/help/context_help_") + lang + QLatin1String(".qm");
    QFile file(trFileName);
    if (file.open(QFile::ReadOnly))
    {
        if (m_translator) {
            QCoreApplication::removeTranslator(m_translator);
            delete m_translator;
        }
        m_translator = new QTranslator();
        if (m_translator->load(trFileName))
            qApp->installTranslator(m_translator);
        else
            qWarning() << "Invalid translation file" << trFileName;
    }
    else {
        qWarning() << "Can not find context help for language" << lang;
    }
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

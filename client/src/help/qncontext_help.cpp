#include "qncontext_help.h"

Q_GLOBAL_STATIC(QnContextHelp, getInstance)

QnContextHelp::QnContextHelp(): 
    m_translator(0),
    m_currentId(ContextId_Invalid)
{
    installHelpContext("en");
    
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
    QStringList data = m_settings.value("shownContext").toString().split(';');
    foreach(const QString& contextId, data)
    {
        if (!contextId.isEmpty())
            m_shownContext[(ContextId) contextId.toInt()] = true;
    }
}

void QnContextHelp::serializeShownContext()
{
    QString rez;
    for(QMap<ContextId, bool>::const_iterator itr = m_shownContext.begin(); itr != m_shownContext.end(); ++itr)
    {
        if (itr.value()) 
        {
            if (!rez.isEmpty())
                rez += ';';
            rez += QString::number((int) itr.key());
        }
    }
    m_settings.setValue("shownContext", rez);
}

void QnContextHelp::installHelpContext(const QString& lang)
{
    QString trFileName = QString(":/help/context_help_") + lang + QString(".qm");
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

bool QnContextHelp::isNeedAutoShow(ContextId id) const
{
    return !m_shownContext[id];
}

void QnContextHelp::setHelpContext(ContextId id)
{
    if(m_currentId == id)
        return;

    m_currentId = id;
    emit helpContextChanged(id);
}

void QnContextHelp::setNeedAutoShow(ContextId id, bool value)
{
    m_shownContext[id] = !value;
    serializeShownContext();
}

void QnContextHelp::resetShownInfo()
{
    m_shownContext.clear();
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
    default:
        qWarning() << "Unknown help context" << (int) m_currentId;
        return QString();
    }
}

#ifndef __QNCONTEXT_HELP_H_
#define __QNCONTEXT_HELP_H_

#include <QCoreApplication>
#include <QSettings>

class QnContextHelp: public QObject
{
    Q_OBJECT;

public:
    enum ContextId {
        ContextId_Scene,
        ContextId_MotionGrid
    };

    static QnContextHelp* instance();

    QnContextHelp();
    ~QnContextHelp();
    void setHelpContext(ContextId id);
    
    void setShown(ContextId id, bool value);
    bool isShown(ContextId id) const;
    void resetShownInfo();
signals:
    void helpContextChanged(ContextId id, const QString& helpText, bool isFirstTime);
private:
    void installHelpContext(const QString& lang);
    void deserializeShownContext();
    void serializeShownContext();
private:
    QTranslator* m_translator;
    QSettings m_settings;
    QMap<ContextId, bool> m_shownContext;
};

#endif // __QNCONTEXT_HELP_H_

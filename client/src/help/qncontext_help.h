#ifndef __QNCONTEXT_HELP_H_
#define __QNCONTEXT_HELP_H_

#include <QCoreApplication>
#include <QSettings>

class QnContextHelp: public QObject
{
    Q_OBJECT;

public:
    enum ContextId {
        ContextId_Invalid = -1,
        ContextId_Scene = 0,
        ContextId_MotionGrid,
        ContextId_Slider,
        ContextId_Tree,
        ContextId_Search
    };

    static QnContextHelp* instance();

    QnContextHelp();
    ~QnContextHelp();
    void setHelpContext(ContextId id);
    
    void setNeedAutoShow(ContextId id, bool value);
    bool isNeedAutoShow(ContextId id) const;

    QString text(ContextId id) const;

    ContextId currentId() const;

    void resetShownInfo();
signals:
    void helpContextChanged(QnContextHelp::ContextId id);
private:
    void installHelpContext(const QString& lang);
    void deserializeShownContext();
    void serializeShownContext();
private:
    ContextId m_currentId;
    QTranslator* m_translator;
    QSettings m_settings;
    QMap<ContextId, bool> m_shownContext;
};

#endif // __QNCONTEXT_HELP_H_

#ifndef REBUILD_ARCHIVE_REPLY_H
#define REBUILD_ARCHIVE_REPLY_H

#include <QtCore/QMetaType>

struct QnRebuildArchiveReply
{
    enum RebuildState
    {
        Unknown,
        Started,
        Stopped
    };

    QnRebuildArchiveReply(): m_progress(0), m_state(Unknown)
    {
    }
    
    void deserialize(const QByteArray& data)
    {
        m_progress = -1;
        int idx1 = data.indexOf("<progress>");
        int idx2 = data.indexOf("</progress>");
        if (idx1 >= 0 && idx2 >= 0 && idx2 > idx1) {
            idx1 += QByteArray("<progress>").length();
            m_progress = data.mid(idx1, idx2 - idx1).toInt();
        }
        m_state = (m_progress >= 0) ? Started : Stopped;

        idx1 = data.indexOf("<state>");
        idx2 = data.indexOf("</state>");
        if (idx1 >= 0 && idx2 >= 0 && idx2 > idx1) {
            idx1 += QByteArray("<state>").length();
            QByteArray stateStr = data.mid(idx1, idx2 - idx1);
            if (stateStr == "none")
                m_state = Stopped;
            else
                m_state = Started;
        }
    }
    
    int progress() const { return m_progress; }
    int state() const { return m_state; }

private:
    int m_progress;
    int m_state;
};

Q_DECLARE_METATYPE(QnRebuildArchiveReply)

#endif // REBUILD_ARCHIVE_REPLY_H

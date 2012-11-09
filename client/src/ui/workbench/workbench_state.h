#ifndef QN_WORKBENCH_STATE_H
#define QN_WORKBENCH_STATE_H

#include <QtCore/QUuid>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>

class QnWorkbenchState {
public:
    QnWorkbenchState(): currentLayoutIndex(-1) {}

    int currentLayoutIndex;
    QList<QString> layoutUuids;
};

inline QDataStream &operator<<(QDataStream &stream, const QnWorkbenchState &state) {
    return stream << state.currentLayoutIndex << state.layoutUuids;
}

inline QDataStream &operator>>(QDataStream &stream, QnWorkbenchState &state) {
    return stream >> state.currentLayoutIndex >> state.layoutUuids;
}

typedef QHash<QString, QnWorkbenchState> QnWorkbenchStateHash;

Q_DECLARE_METATYPE(QnWorkbenchState);
Q_DECLARE_METATYPE(QnWorkbenchStateHash);

#endif // QN_WORKBENCH_STATE_H


#pragma once

#include <QUuid>
#include <QObject>
#include <QVector>
#include <QString>
#include <QScopedPointer>

#include <memory>
#include <functional>

namespace rtu
{
    typedef QHash<int, QByteArray> Roles;

    class ServerInfo;
    typedef QVector<ServerInfo *> ServerInfoPtrContainer;
    typedef QVector<ServerInfo> ServerInfoContainer;

    typedef QVector<QUuid> IDsVector;

    ///

    typedef QSharedPointer<QString> StringPointer;
    typedef QSharedPointer<int> IntPointer;
    typedef QSharedPointer<bool> BoolPointer;

    typedef std::function<void ()> Callback;

    class ApplyChangesTask;
    typedef std::unique_ptr<ApplyChangesTask> ApplyChangesTaskPtr;

    class ChangesProgressModel;
    typedef std::unique_ptr<ChangesProgressModel> ChangesProgressModelPtr;
}

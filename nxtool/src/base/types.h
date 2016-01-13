
#pragma once

#include <QUuid>
#include <QObject>
#include <QVector>
#include <QString>

#include <memory>
#include <functional>

class QDateTime;

namespace rtu
{
    typedef QHash<int, QByteArray> Roles;

    typedef QVector<QUuid> IDsVector;

    class Changeset;
    typedef std::shared_ptr<Changeset> ChangesetPointer;

    ///
    struct DateTime;

    typedef std::shared_ptr<DateTime> DateTimePointer;
    typedef std::shared_ptr<QString> StringPointer;
    typedef std::shared_ptr<int> IntPointer;
    typedef std::shared_ptr<bool> BoolPointer;
    typedef std::function<void ()> Callback, Action;

    class ApplyChangesTask;
    typedef std::shared_ptr<ApplyChangesTask> ApplyChangesTaskPtr;

    class ChangesProgressModel;
    typedef std::unique_ptr<ChangesProgressModel> ChangesProgressModelPtr;
}

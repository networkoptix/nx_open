#include "workbench_debug_handler.h"

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>

#include <ui/actions/action.h>
#include <ui/actions/action_manager.h>
#include <ui/widgets/palette_widget.h>
#include <ui/dialogs/common/dialog.h>
#include <ui/dialogs/resource_list_dialog.h>
#include <ui/workaround/qtbug_workaround.h>

// -------------------------------------------------------------------------- //
// QnDebugControlDialog
// -------------------------------------------------------------------------- //
class QnDebugControlDialog: public QnDialog, public QnWorkbenchContextAware {
    typedef QnDialog base_type;

public:
    QnDebugControlDialog(QWidget *parent = NULL):
        base_type(parent),
        QnWorkbenchContextAware(parent)
    {
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(newActionButton(QnActions::DebugDecrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugIncrementCounterAction));
        layout->addWidget(newActionButton(QnActions::DebugShowResourcePoolAction));
        setLayout(layout);
    }

private:
    QToolButton *newActionButton(QnActions::IDType actionId, QWidget *parent = NULL) {
        QToolButton *button = new QToolButton(parent);
        button->setDefaultAction(menu()->action(actionId));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        return button;
    }
};


// -------------------------------------------------------------------------- //
// QnWorkbenchDebugHandler
// -------------------------------------------------------------------------- //
QnWorkbenchDebugHandler::QnWorkbenchDebugHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(QnActions::DebugControlPanelAction),                &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered);
    connect(action(QnActions::DebugIncrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered);
    connect(action(QnActions::DebugDecrementCounterAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered);
    connect(action(QnActions::DebugShowResourcePoolAction),            &QAction::triggered,    this,   &QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered);
}

void QnWorkbenchDebugHandler::at_debugControlPanelAction_triggered() {
    QScopedPointer<QnDebugControlDialog> dialog(new QnDebugControlDialog(mainWindow()));
    dialog->exec();
}

void QnWorkbenchDebugHandler::at_debugIncrementCounterAction_triggered() {
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() + 1);
    qDebug() << qnRuntime->debugCounter();

    auto showPalette = [this] {
        QnPaletteWidget *w = new QnPaletteWidget();
        w->setPalette(qApp->palette());
        w->show();
    };

    // the palette widget code maight still be required
    Q_UNUSED(showPalette);
}

#include <common/common_module.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/util.h>


QStringList getRandomStrings(const QStringList &dict, int len, int multiplier) {

    QStringList result;
    int min = 1;
    int max = dict.size() / multiplier;

    for (int i = 0; i < len; ++i) {
        int j = random(min, max);
        result << dict[j * multiplier];
    }

    return result;
}


QString getRandomName(const QStringList &dict) {
    return getRandomStrings(dict, 2, 109).join(L' ');
}

QString getRandomDescription(const QStringList &dict) {
    return getRandomStrings(dict, 20, 7).join(L' ');
}

QnCameraBookmarkTags getRandomTags(const QStringList &dict) {
    return getRandomStrings(dict, 6, 113).toSet();
}

qint64 move() {
    return random(100000, 300000);
}

void QnWorkbenchDebugHandler::at_debugDecrementCounterAction_triggered()
{
#ifdef _DEBUG
    qnRuntime->setDebugCounter(qnRuntime->debugCounter() - 1);
    qDebug() << qnRuntime->debugCounter();

    QList<qint64> steps;
    steps << 1000 << 4000 << 16000 << 64000 << 256000;

    QStringList dict;
    QFile dictFile(lit("c:/sandbox/5000.dic"));
    dictFile.open(QFile::ReadOnly);
    QTextStream stream(&dictFile);
    while (!stream.atEnd()) {
        dict << stream.readLine();
    }

    int skip = 10;
    int limit = 16000;
    int counter = 0;

    auto server = qnCommon->currentServer();
    for (const QnVirtualCameraResourcePtr &camera: qnResPool->getAllCameras(server, true)) {
        qDebug() << "Camera" << camera->getName();

        QnCachingCameraDataLoader *loader = context()->instance<QnCameraDataManager>()->loader(camera, false);
        if (!loader)
            continue;

        auto periods = loader->periods(Qn::RecordingContent);
        for (const QnTimePeriod &period: periods) {
            qDebug() << "period" << period;

            qint64 start = period.startTimeMs;
            qint64 end = period.isInfinite()
                ? QDateTime::currentMSecsSinceEpoch()
                : period.endTimeMs();

            while (start < end) {
                for (qint64 step: steps) {
                    if (start + step > end)
                        break;

                    ++counter;
                    if (counter < skip) {
                        if (counter % 500 == 0)
                            qDebug() << "skipping counter" << counter;
                        continue;
                    }
                    if (counter > limit)
                        return;

                    QnCameraBookmark bookmark;
                    bookmark.guid = QnUuid::createUuid();
                    bookmark.startTimeMs = start;
                    bookmark.durationMs = step;
                    bookmark.name = getRandomName(dict);
                    bookmark.description = getRandomDescription(dict);
                    bookmark.tags = getRandomTags(dict);
                    bookmark.cameraId = camera->getUniqueId();

                    QDateTime startDt = QDateTime::fromMSecsSinceEpoch(start);
                    bookmark.timeout = startDt.addMonths(6).toMSecsSinceEpoch() - start;
                    qDebug() << "adding bookmark " << counter << QDateTime::fromMSecsSinceEpoch(start);

                    qnCameraBookmarksManager->addCameraBookmark(bookmark, [this, start](bool success) {
                        qDebug() << "bookmark added " << success << QDateTime::fromMSecsSinceEpoch(start);
                    });

                    if (counter % 10 == 0)
                        qApp->processEvents();
                }

                start += move();
            }

        }


    }
#endif //_DEBUG
}

void QnWorkbenchDebugHandler::at_debugShowResourcePoolAction_triggered() {
    QScopedPointer<QnResourceListDialog> dialog(new QnResourceListDialog(mainWindow()));
    dialog->setResources(resourcePool()->getResources());
    dialog->exec();
}

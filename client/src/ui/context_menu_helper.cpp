#include "context_menu_helper.h"

#include <QtGui/QKeySequence>
#include <QtGui/QMenu>

#include "ui/skin/skin.h"

QAction cm_new_item(QObject::tr("New Item"), 0);
QAction cm_exit(QObject::tr("Exit"), 0);
QAction cm_fitinview(QObject::tr("Fit in View"), 0);
QAction cm_open_file(QObject::tr("Open file(s)..."), 0);
QAction cm_screen_recording(QObject::tr("Screen Recording"), 0);
QAction cm_start_video_recording(QObject::tr("Start/Stop Recording"), 0);
QAction cm_recording_settings(QObject::tr("Recording settings"), 0);
QAction cm_toggle_fullscreen(QObject::tr("Toggle fullscreen"), 0);
QAction cm_arrange(QObject::tr("Arrange"), 0);
QAction cm_add_layout(QObject::tr("Add new layout..."), 0);
QAction cm_restore_layout(QObject::tr("Restore layout"), 0);
QAction cm_save_layout(QObject::tr("Save layout"), 0);
QAction cm_save_layout_as(QObject::tr("Save layout as..."), 0);
QAction cm_preferences(QObject::tr("Preferences"), 0);

QAction cm_editTags(QObject::tr("Edit tags..."), 0);

QAction cm_fullscreen(QObject::tr("Fullscreen"), 0);
QAction cm_remove_from_layout(QObject::tr("Remove"), 0);
QAction cm_remove_from_disk(QObject::tr("Delete file(s)"), 0);
QAction cm_settings(QObject::tr("Settings..."), 0);
QAction cm_start_recording(QObject::tr("Start recording"), 0);
QAction cm_stop_recording(QObject::tr("Stop recording"), 0);
QAction cm_view_recorded(QObject::tr("View recorded video"), 0);
QAction cm_save_recorded_as(QObject::tr("Save recorded video as..."), 0);
QAction cm_take_screenshot(QObject::tr("Take screenshot..."), 0);
QAction cm_upload_youtube(QObject::tr("Upload to YouTube..."), 0);
QAction cm_open_web_page(QObject::tr("Open web page(experimental)"), 0);

QAction cm_distance(QObject::tr("Item distance"), 0);

QAction cm_layout_editor_editlayout(QObject::tr("Edit Layout..."), 0);
QAction cm_layout_editor_bgp(QObject::tr("Change BackGround picture..."), 0);
QAction cm_layout_editor_bgp_sz(QObject::tr("Change BackGround picture size..."), 0);
QAction cm_layout_editor_change_t(QObject::tr("Change layout title..."), 0);

QAction cm_rotate(QObject::tr("Rotation"), 0);

QAction cm_open_containing_folder(QObject::tr("Open in containing folder..."), 0);

void initContextMenu()
{
    cm_exit.setIcon(Skin::icon(QLatin1String("decorations/exit-application.png")));
    cm_exit.setShortcut(QObject::tr("Alt+F4"));
    cm_exit.setShortcutContext(Qt::ApplicationShortcut);

    cm_preferences.setIcon(Skin::icon(QLatin1String("decorations/settings.png")));
    cm_preferences.setShortcut(QObject::tr("Ctrl+P"));
    cm_preferences.setShortcutContext(Qt::ApplicationShortcut);

    cm_open_file.setShortcut(QObject::tr("Ctrl+O"));
    cm_open_file.setShortcutContext(Qt::ApplicationShortcut);

    cm_remove_from_disk.setShortcut(QObject::tr("Shift+Del"));
    cm_remove_from_disk.setShortcutContext(Qt::ApplicationShortcut);

    cm_open_containing_folder.setShortcut(QObject::tr("Ctrl+Return"));
    cm_open_containing_folder.setShortcutContext(Qt::ApplicationShortcut);

    cm_upload_youtube.setShortcut(QObject::tr("Ctrl+Y"));
    cm_upload_youtube.setShortcutContext(Qt::ApplicationShortcut);

    cm_editTags.setShortcut(QObject::tr("Alt+T"));
    cm_editTags.setShortcutContext(Qt::ApplicationShortcut);

    QMenu *newItemMenu = new QMenu(QObject::tr("New Item"));
    newItemMenu->addAction(new QAction(QObject::tr("Camera"), &cm_new_item));
    newItemMenu->addAction(new QAction(QObject::tr("Media Folder"), &cm_new_item));
    newItemMenu->addAction(new QAction(QObject::tr("Layout"), &cm_new_item));
    cm_new_item.setMenu(newItemMenu);
    cm_new_item.setIcon(Skin::icon(QLatin1String("plus.png")));
    cm_new_item.setToolTip(QObject::tr("New Item"));

    cm_start_video_recording.setShortcuts(QList<QKeySequence>() << QObject::tr("Alt+R") << Qt::Key_MediaRecord);
    cm_start_video_recording.setShortcutContext(Qt::ApplicationShortcut);

    QMenu *screenRecordingMenu = new QMenu(QObject::tr("Screen Recording"));
    screenRecordingMenu->addAction(&cm_start_video_recording);
    screenRecordingMenu->addAction(&cm_recording_settings);
    cm_screen_recording.setMenu(screenRecordingMenu);
    cm_screen_recording.setToolTip(QObject::tr("Screen Recording"));

    QList<QKeySequence> shortcuts;
#ifdef Q_OS_MAC
    shortcuts << QObject::tr("Ctrl+F");
#else
    shortcuts << QObject::tr("Alt+Return") << QObject::tr("Esc");
#endif
    cm_toggle_fullscreen.setShortcuts(shortcuts);
    cm_toggle_fullscreen.setShortcutContext(Qt::ApplicationShortcut);
    cm_toggle_fullscreen.setIcon(Skin::icon(QLatin1String("decorations/togglefullscreen.png")));

    QMenu *itemDistanceMenu = new QMenu(QObject::tr("Item distance"));
    for (int i = 5; i < 40; i += 5)
    {
        QAction *action = new QAction(QString("%1%").arg(i), &cm_distance);
        action->setData(float(i) / 100);
        itemDistanceMenu->addAction(action);
    }
    cm_distance.setMenu(itemDistanceMenu);
    cm_distance.setToolTip(QObject::tr("Item distance"));

    QMenu *rotationMenu = new QMenu(QObject::tr("Rotation"));
    {
        QAction *action = new QAction(QObject::tr("0"), &cm_rotate);
        action->setData(0);
        rotationMenu->addAction(action);
    }
    {
        QAction *action = new QAction(QObject::tr("90"), &cm_rotate);
        action->setData(90);
        rotationMenu->addAction(action);
    }
    {
        QAction *action = new QAction(QObject::tr("-90"), &cm_rotate);
        action->setData(-90);
        rotationMenu->addAction(action);
    }
    {
        QAction *action = new QAction(QObject::tr("180"), &cm_rotate);
        action->setData(-180);
        rotationMenu->addAction(action);
    }
    cm_rotate.setMenu(rotationMenu);
    cm_rotate.setToolTip(QObject::tr("Rotation"));
}

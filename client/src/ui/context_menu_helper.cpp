#include "context_menu_helper.h"

#include <QtGui/QKeySequence>
#include <QtGui/QMenu>

#include "ui/style/skin.h"

#include "ui/context_menu/menu_wrapper.h"

QAction cm_new_item(QObject::tr("New Item..."), 0);
QAction cm_about(QObject::tr("About..."), 0);
QAction cm_exit(QObject::tr("Exit"), 0);
QAction cm_reconnect(QObject::tr("Reconnect"), 0);
QAction cm_fitinview(QObject::tr("Fit in View"), 0);
QAction cm_open_file(QObject::tr("Open file(s)..."), 0);
QAction cm_screen_recording(QObject::tr("Screen Recording"), 0);
QAction cm_toggle_recording(QObject::tr("Start Screen Recording"), 0);
QAction cm_recording_settings(QObject::tr("Screen Recording Settings"), 0);
QAction cm_toggle_fullscreen(QObject::tr("Toggle fullscreen"), 0);
QAction cm_toggle_fps(QObject::tr("Toggle FPS display"), 0);
QAction cm_arrange(QObject::tr("Arrange"), 0);
QAction cm_add_layout(QObject::tr("Add new layout..."), 0);
QAction cm_restore_layout(QObject::tr("Restore layout"), 0);
QAction cm_save_layout(QObject::tr("Save layout"), 0);
QAction cm_save_layout_as(QObject::tr("Save layout as..."), 0);
QAction cm_preferences(QObject::tr("Preferences"), 0);

QAction cm_new_tab(QObject::tr("New Tab"), 0);

QAction cm_hide_decorations(QObject::tr("Hide decorations"), 0);

QAction cm_editTags(QObject::tr("Edit tags..."), 0);

QAction cm_fullscreen(QObject::tr("Fullscreen"), 0);
QAction cm_remove_from_layout(QObject::tr("Remove"), 0);
QAction cm_remove_from_disk(QObject::tr("Delete file(s)"), 0);
QAction cm_settings(QObject::tr("Settings..."), 0);
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

QAction cm_showNavTree(QObject::tr("<=|=>"), 0);

void initContextMenu()
{
    cm_new_tab.setToolTip(QObject::tr("New Tab"));
    cm_new_tab.setShortcut(QKeySequence::New);
    cm_new_tab.setIcon(Skin::icon(QLatin1String("plus.png")));

    cm_about.setIcon(Skin::icon(QLatin1String("info.png")));
    cm_about.setMenuRole(QAction::AboutRole);
    cm_about.setAutoRepeat(false);

    cm_exit.setIcon(Skin::icon(QLatin1String("decorations/exit-application.png")));
    cm_exit.setShortcut(QObject::tr("Alt+F4"));
    cm_exit.setShortcutContext(Qt::ApplicationShortcut);
    cm_exit.setMenuRole(QAction::QuitRole);
    cm_exit.setAutoRepeat(false);

    cm_reconnect.setIcon(Skin::icon(QLatin1String("connect.png")));
    cm_reconnect.setAutoRepeat(false);

    cm_preferences.setIcon(Skin::icon(QLatin1String("decorations/settings.png")));
    cm_preferences.setShortcut(QObject::tr("Ctrl+P"));
    cm_preferences.setShortcutContext(Qt::ApplicationShortcut);
    cm_preferences.setMenuRole(QAction::PreferencesRole);
    cm_preferences.setAutoRepeat(false);

    cm_open_file.setIcon(Skin::icon(QLatin1String("folder.png")));
    cm_open_file.setShortcut(QObject::tr("Ctrl+O"));
    cm_open_file.setShortcutContext(Qt::ApplicationShortcut);
    cm_open_file.setAutoRepeat(false);

    cm_remove_from_disk.setShortcut(QObject::tr("Shift+Del"));
    cm_remove_from_disk.setShortcutContext(Qt::ApplicationShortcut);
    cm_remove_from_disk.setAutoRepeat(false);

    cm_open_containing_folder.setShortcut(QObject::tr("Ctrl+Return"));
    cm_open_containing_folder.setShortcutContext(Qt::ApplicationShortcut);
    cm_open_containing_folder.setAutoRepeat(false);

    cm_upload_youtube.setShortcut(QObject::tr("Ctrl+Y"));
    cm_upload_youtube.setShortcutContext(Qt::ApplicationShortcut);
    cm_upload_youtube.setAutoRepeat(false);

    cm_editTags.setShortcut(QObject::tr("Alt+T"));
    cm_editTags.setShortcutContext(Qt::ApplicationShortcut);
    cm_editTags.setAutoRepeat(false);

    cm_hide_decorations.setShortcut(QObject::tr("F11"));
    cm_hide_decorations.setShortcutContext(Qt::ApplicationShortcut);
    cm_hide_decorations.setAutoRepeat(false);

    QMenu *newItemMenu = new QMenu();
    newItemMenu->setTitle(QObject::tr("New Item"));
    newItemMenu->addAction(new QAction(QObject::tr("Camera"), &cm_new_item));
    newItemMenu->addAction(new QAction(QObject::tr("Media Folder"), &cm_new_item));
    newItemMenu->addAction(new QAction(QObject::tr("Layout"), &cm_new_item));
    cm_new_item.setMenu(newItemMenu);
    cm_new_item.setIcon(Skin::icon(QLatin1String("plus.png")));
    cm_new_item.setToolTip(QObject::tr("New Item"));

    cm_toggle_recording.setShortcuts(QList<QKeySequence>() << QObject::tr("Alt+R") << Qt::Key_MediaRecord);
    cm_toggle_recording.setShortcutContext(Qt::ApplicationShortcut);
    cm_toggle_recording.setAutoRepeat(false);

    cm_recording_settings.setAutoRepeat(false);

    QMenu *screenRecordingMenu = new QMenu();
    screenRecordingMenu->setTitle(QObject::tr("Screen Recording"));
    screenRecordingMenu->addAction(&cm_toggle_recording);
    screenRecordingMenu->addAction(&cm_recording_settings);
    cm_screen_recording.setMenu(screenRecordingMenu);
    cm_screen_recording.setToolTip(QObject::tr("Screen Recording"));

    cm_toggle_fullscreen.setIcon(Skin::icon(QLatin1String("decorations/fullscreen.png")));
    QList<QKeySequence> shortcuts;
#ifdef Q_OS_MAC
    shortcuts << QObject::tr("Ctrl+F");
#else
    shortcuts << QObject::tr("Alt+Return") << QObject::tr("Esc");
#endif
    cm_toggle_fullscreen.setShortcuts(shortcuts);
    cm_toggle_fullscreen.setShortcutContext(Qt::ApplicationShortcut);
    cm_toggle_fullscreen.setAutoRepeat(false);

    cm_toggle_fps.setShortcut(QObject::tr("Ctrl+Alt+F"));
    cm_toggle_fps.setShortcutContext(Qt::ApplicationShortcut);
    cm_toggle_fps.setCheckable(true);
    cm_toggle_fps.setAutoRepeat(false);

    QMenu *itemDistanceMenu = new QMenu();
    itemDistanceMenu->setTitle(QObject::tr("Item distance"));
    for (int i = 5; i < 40; i += 5)
    {
        QAction *action = new QAction(QString("%1%").arg(i), &cm_distance);
        action->setData(float(i) / 100);
        itemDistanceMenu->addAction(action);
    }
    cm_distance.setMenu(itemDistanceMenu);
    cm_distance.setToolTip(QObject::tr("Item distance"));

    QMenu *rotationMenu = new QMenu();
    rotationMenu->setTitle(QObject::tr("Rotation"));
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


    cm_showNavTree.setAutoRepeat(false);
    cm_showNavTree.setToolTip(QObject::tr("Toggle navigation tree show/hide"));
}

#include "context_menu_helper.h"

QAction cm_exit(QObject::tr("Exit"), 0);
QAction cm_fitinview(QObject::tr("Fit in View"), 0);
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
QAction cm_remove_from_disk(QObject::tr("Remove file(s)"), 0);
QAction cm_settings(QObject::tr("Settings..."), 0);
QAction cm_start_recording(QObject::tr("Start recording"), 0);
QAction cm_stop_recording(QObject::tr("Stop recording"), 0);
QAction cm_view_recorded(QObject::tr("View recorded video"), 0);
QAction cm_save_recorded_as(QObject::tr("Save recorded video as..."), 0);
QAction cm_open_web_page(QObject::tr("Open web page(experimental)"), 0);

QAction dis_0(QObject::tr("0%"), 0);
QAction dis_5(QObject::tr("5%"), 0);
QAction dis_10(QObject::tr("10%"), 0);
QAction dis_15(QObject::tr("15%"), 0);
QAction dis_20(QObject::tr("20%"), 0);
QAction dis_25(QObject::tr("25%"), 0);
QAction dis_30(QObject::tr("30%"), 0);
QAction dis_35(QObject::tr("35%"), 0);

QAction cm_layout_editor_editlayout(QObject::tr("Edit Layout..."), 0);
QAction cm_layout_editor_bgp(QObject::tr("Change BackGround picture..."), 0);
QAction cm_layout_editor_bgp_sz(QObject::tr("Change BackGround picture size..."), 0);
QAction cm_layout_editor_change_t(QObject::tr("Change layout title..."), 0);

QAction cm_rotate_90(QObject::tr("90"), 0);
QAction cm_rotate_0(QObject::tr("0"), 0);
QAction cm_rotate_minus90(QObject::tr("-90"), 0);
QAction cm_rotate_180(QObject::tr("180"), 0);

QAction cm_open_containing_folder(QObject::tr("Open in containing folder..."), 0);

void initContextMenu()
{
    dis_0.setData(0.0);
    dis_5.setData(0.05);
    dis_10.setData(0.10);
    dis_15.setData(0.15);
    dis_20.setData(0.20);
    dis_25.setData(0.25);
    dis_30.setData(0.30);
    dis_35.setData(0.35);
}

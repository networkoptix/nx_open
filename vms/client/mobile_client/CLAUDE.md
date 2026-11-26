# CLAUDE.md

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

## C++ backend

### QML type -> C++ class

All headers relative to `src/`.

| QML type | C++ header | What it does |
| --- | --- | --- |
| CameraButtonController | `nx/vms/client/mobile/camera/buttons/camera_button_controller.h` | Soft triggers, audio, PTZ, I/O buttons |
| CameraButtonsModel | `nx/vms/client/mobile/camera/buttons/camera_buttons_model.h` | List model for action buttons |
| PtzController | `nx/mobile_client/controllers/resource_ptz_controller.h` | Pan-tilt-zoom control |
| PtzPresetModel | `nx/mobile_client/models/ptz_preset_model.h` | PTZ preset list |
| AudioController | `nx/mobile_client/controllers/audio_controller.h` | Two-way audio per camera |
| SessionManager | `nx/vms/client/mobile/session/session_manager.h` | Server connection sessions |
| MediaDownloadBackend | `nx/vms/client/mobile/camera/media_download_backend.h` | Video export/download |
| ShareBookmarkBackend | `nx/vms/client/mobile/camera/share_bookmark_backend.h` | Bookmark sharing |
| PushNotificationProvider | `nx/vms/client/mobile/push_notification/push_notification_provider.h` | Push notification data |
| PushNotificationManager | `nx/vms/client/mobile/push_notification/push_notification_manager.h` | Push notification subscriptions |
| ObjectsLoader | `nx/vms/client/mobile/timeline/objects_loader.h` | Timeline objects (events, bookmarks, analytics) |
| OrganizationTreeModel | `nx/vms/client/mobile/models/organization_tree_model.h` | Cloud org hierarchy |
| ResourceTreeModel | `nx/vms/client/mobile/models/resource_tree_model.h` | Resource tree (cameras, devices) |
| QnCameraListModel | `models/camera_list_model.h` | Camera list with filtering |
| VoiceSpectrumItem | `nx/client/mobile/two_way_audio/voice_spectrum_item.h` | Audio spectrum viz |

Core types (not mobile-specific, under `open/vms/client/nx_vms_client_core/`): MediaPlayer and
ChunkProvider in the QML module `nx.vms.client.core`, ResourceHelper in `Nx.Core`.

### Legacy code (older namespaces)

- `src/nx/mobile_client/controllers/` -- PtzController, AudioController
- `src/nx/client/mobile/motion/` -- motion masks, chunk watchers
- `src/nx/client/mobile/two_way_audio/` -- voice spectrum visualization

## UI Map

All paths relative to `static-resources/qml/Nx/`. Private subcomponents under `Screens/private/<ScreenName>/`.

### App shell

- Layout mode (horizontal compact / compact / medium / expanded / unsupported, by window size) -> `Ui/LayoutController.qml`, `Ui/LayoutMode.qml`
- Screen container -> `Ui/UiContainer.qml`
- Base screen with left/right panels, splash -> `Items/AdaptiveScreen.qml`
- Too-small window notice (Unsupported layout mode hides the screens, they keep the last supported layout and interrupt the video) -> `Ui/UiContainer.qml`
- Side panel -> `Items/Panel.qml`
- Top bar -> `Controls/ToolBar.qml`
- Bottom navigation -> `Controls/ScreenNavigationBar.qml`
- Snackbar -> `Controls/SnackBar.qml`

### Sessions Screen -- `Screens/SessionsScreen.qml`

Also known as: Home Screen, Welcome Screen, Sites Tab, Server List

- Site list -> `Items/SiteList.qml`
- Organization tree (tablet) -> `private/SessionsScreen/OrganizationTreeView.qml`
- Connect to server -> `Screens/CustomConnectionScreen.qml`
- Cloud login -> `Screens/Cloud/Login.qml`

### Resources Screen -- `Screens/ResourcesScreen.qml`

Also known as: Camera Grid, Devices Grid, Camera List

- Cameras grid -> `private/ResourcesScreen/CamerasGrid.qml`
- Camera tile -> `private/ResourcesScreen/CameraItem.qml`
- Resource tree -> `Items/ResourceTreeItem.qml`

### Video Screen -- `Screens/VideoScreen.qml`

Also known as: Camera Screen, Live View, Video View

- Video player -> `private/VideoScreen/ScalableVideo.qml`
- Controller -> `private/VideoScreen/VideoScreenController.qml`
- Timeline -> `private/VideoScreen/DataTimeline.qml`
- PTZ panel -> `private/VideoScreen/PtzPanel.qml`, `private/VideoScreen/Ptz/Joystick.qml`
- Action buttons (soft triggers, two-way audio) -> `private/VideoScreen/ActionSheet.qml`
- Export -> `Mobile/Ui/Sheets/DownloadMediaDurationSheet.qml`

### Feed Screen -- `Screens/FeedScreen.qml`

Also known as: Notifications Screen, Notifications Tab

- Notification list -> `private/FeedScreen/Feed.qml`
- Notification item -> `private/FeedScreen/Notification.qml`
- Feed state -> `Ui/FeedStateProvider.qml`

### Event Search Screen -- `Screens/EventSearch/EventSearchScreen.qml`

Also known as: Bookmark List, Object Search, Smart Search

- Search results -> `Screens/EventSearch/private/items/EventSearchItem.qml`
- Filters -> `Screens/EventSearch/private/items/FiltersItem.qml`
- Sharing -> `Mobile/Ui/Sheets/ShareBookmarkSheet.qml`

### Menu Screen -- `Screens/MenuScreen.qml`

Also known as: Side Menu, Burger Menu

### Settings Screen -- `Screens/SettingsScreen.qml`

Subpages under `private/SettingsScreen/`: InterfaceSettingsPage, PerformanceSettingsPage, SecuritySettingsPage, PushExpertModePage, BetaFeaturesPage, DeveloperSettingsPage, AppInfoPage.

### Non-obvious shared controls

- Sheet (bottom/right) -> `Mobile/Controls/AdaptiveSheet.qml`
- Spinner -> `Controls/ActivityPreloader.qml`, `Controls/CircleBusyIndicator.qml`
- Skeleton loader -> `Items/Skeleton.qml`
- Popup -> `Mobile/Popups/PopupBase.qml`, `Mobile/Popups/StandardPopup.qml`

### Dialogs

- SSL certificate -> `Web/UnknownSslCertificateDialog.qml`, `Web/InvalidOrChangedCertificateDialog.qml`
- External link -> `Web/LinkAboutToOpenDialog.qml`
- 2FA error -> `Web/TwoFactorAuthenticationErrorDialog.qml`

## Tests

- C++ unit tests (GoogleTest): `unit_tests/`, target `mobile_client_ut`. Fixture with the
  application context in the unit tests mode:
  `unit_tests/src/nx/vms/client/mobile/test_support/test_context.h`.
- QML tests (Qt Quick Test): `unit_tests/qml/`, target `mobile_client_qml_ut`. Test files are
  `unit_tests/qml/static-resources/tests/<module path>/tst_*.qml`. See `unit_tests/qml/readme.md`
  for the conventions, the available environment and how to run them.

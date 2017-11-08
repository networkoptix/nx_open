#pragma once

#include <utils/common/property_storage.h>
#include <nx/utils/singleton.h>

class QnClientRuntimeSettings: public QnPropertyStorage, public Singleton<QnClientRuntimeSettings> {
    Q_OBJECT

    typedef QnPropertyStorage base_type;
public:
    enum Variable {
        /** YUV-shader emulation mode. */
        SOFTWARE_YUV,

        /** Debug value. */
        DEBUG_COUNTER,

        /** Developers mode with additional options */
        DEV_MODE,

        /** If does not equal -1 then its value will be copied to LIGHT_MODE.
         *  It should be set from command-line and it disables light mode auto detection. */
        LIGHT_MODE_OVERRIDE,

        /** Videowall client mode - client should not be controlled manually. */
        VIDEO_WALL_MODE,

        /** ActiveX library mode - client is embedded to another window. */
        ACTIVE_X_MODE,

        /** Always display full info on cameras. */
        SHOW_FULL_INFO,

        /** Do not notify users if servers have different versions. */
        IGNORE_VERSION_MISMATCH,

        /** Use OpenGL double buffering. */
        GL_DOUBLE_BUFFER,

        /** Current locale. */
        LOCALE,

        /** Enable profiler mode with some additional logs and vsync workaround disabled. */
        PROFILER_MODE,

        // Maximum simultaneous scene items overridden value. 0 means default.
        MAX_SCENE_ITEMS_OVERRIDE,

        VARIABLE_COUNT
    };

    explicit QnClientRuntimeSettings(QObject *parent = nullptr);
    ~QnClientRuntimeSettings();

    bool isDesktopMode() const;

    /**
     * Actual maximum value of scene items.
     */
    int maxSceneItems() const;

private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(bool,    isSoftwareYuv,          setSoftwareYuv,             SOFTWARE_YUV,               false)
        QN_DECLARE_RW_PROPERTY(int,     debugCounter,           setDebugCounter,            DEBUG_COUNTER,              0)
        QN_DECLARE_RW_PROPERTY(bool,    isDevMode,              setDevMode,                 DEV_MODE,                   false)
        QN_DECLARE_RW_PROPERTY(int,     lightModeOverride,      setLightModeOverride,       LIGHT_MODE_OVERRIDE,        -1)
        QN_DECLARE_RW_PROPERTY(bool,    isVideoWallMode,        setVideoWallMode,           VIDEO_WALL_MODE,            false)
        QN_DECLARE_RW_PROPERTY(bool,    isActiveXMode,          setActiveXMode,             ACTIVE_X_MODE,              false)
        QN_DECLARE_RW_PROPERTY(bool,    showFullInfo,           setShowFullInfo,            SHOW_FULL_INFO,             false)
        QN_DECLARE_RW_PROPERTY(bool,    ignoreVersionMismatch,  setIgnoreVersionMismatch,   IGNORE_VERSION_MISMATCH,    false)
        QN_DECLARE_RW_PROPERTY(bool,    isGlDoubleBuffer,       setGLDoubleBuffer,          GL_DOUBLE_BUFFER,           true)
        QN_DECLARE_RW_PROPERTY(QString, locale,                 setLocale,                  LOCALE,                     QString())
        QN_DECLARE_RW_PROPERTY(bool,    isProfilerMode,         setProfilerMode,            PROFILER_MODE,              false)
        QN_DECLARE_RW_PROPERTY(int,     maxSceneItemsOverride,  setMaxSceneItemsOverride,   MAX_SCENE_ITEMS_OVERRIDE,   0)
    QN_END_PROPERTY_STORAGE()

};

#define qnRuntime QnClientRuntimeSettings::instance()


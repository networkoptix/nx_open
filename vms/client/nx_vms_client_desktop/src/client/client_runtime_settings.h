#pragma once

#include <utils/common/property_storage.h>

#include <nx/utils/singleton.h>

#include "client_globals.h"

struct QnStartupParameters;

class NX_VMS_CLIENT_DESKTOP_API QnClientRuntimeSettings:
    public QnPropertyStorage,
    public Singleton<QnClientRuntimeSettings>
{
    Q_OBJECT

    typedef QnPropertyStorage base_type;
public:
    enum Variable {
        /** YUV-shader emulation mode. */
        SOFTWARE_YUV,

        /** Debug value. */
        DEBUG_COUNTER,

        /** Light client mode - no animations, no background, no opacity, no notifications, 1 camera only allowed. */
        LIGHT_MODE,

        /** If does not equal -1 then its value will be copied to LIGHT_MODE.
         *  It should be set from command-line and it disables light mode auto detection. */
        LIGHT_MODE_OVERRIDE,

        /** Videowall client mode - client should not be controlled manually. */
        VIDEO_WALL_MODE,

        /** Allow timeline to be displayed on the Video Wall. */
        VIDEO_WALL_WITH_TIMELINE,

        /** ACS mode - client is run as a separate camera view-only window. */
        ACS_MODE,

        /** Always display full info on cameras. */
        SHOW_FULL_INFO,

        /** Use OpenGL double buffering. */
        GL_DOUBLE_BUFFER,

        /** Current locale. */
        LOCALE,

        /** Maximum simultaneous scene items overridden value. 0 means default. */
        MAX_SCENE_ITEMS_OVERRIDE,

        /** Allow client updates. */
        ALLOW_CLIENT_UPDATE,

        VARIABLE_COUNT
    };

    explicit QnClientRuntimeSettings(
        const QnStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnClientRuntimeSettings();

    bool isDesktopMode() const;

    /**
     * Actual maximum value of scene items.
     */
    int maxSceneItems() const;

private:
    QN_BEGIN_PROPERTY_STORAGE(VARIABLE_COUNT)
        QN_DECLARE_RW_PROPERTY(bool, isSoftwareYuv, setSoftwareYuv, SOFTWARE_YUV, false)
        QN_DECLARE_RW_PROPERTY(int, debugCounter, setDebugCounter, DEBUG_COUNTER, 0)
        QN_DECLARE_RW_PROPERTY(Qn::LightModeFlags, lightMode, setLightMode, LIGHT_MODE, {})
        QN_DECLARE_RW_PROPERTY(int, lightModeOverride, setLightModeOverride, LIGHT_MODE_OVERRIDE,
            -1)
        QN_DECLARE_RW_PROPERTY(bool, isVideoWallMode, setVideoWallMode, VIDEO_WALL_MODE, false)
        QN_DECLARE_RW_PROPERTY(bool, videoWallWithTimeline, setVideoWallWithTimeLine,
            VIDEO_WALL_WITH_TIMELINE, true)
        QN_DECLARE_RW_PROPERTY(bool, isAcsMode, setAcsMode, ACS_MODE, false)
        QN_DECLARE_RW_PROPERTY(bool, showFullInfo, setShowFullInfo, SHOW_FULL_INFO, false)
        QN_DECLARE_RW_PROPERTY(bool, isGlDoubleBuffer, setGLDoubleBuffer, GL_DOUBLE_BUFFER, true)
        QN_DECLARE_RW_PROPERTY(QString, locale, setLocale, LOCALE, QString())
        QN_DECLARE_RW_PROPERTY(int, maxSceneItemsOverride, setMaxSceneItemsOverride,
            MAX_SCENE_ITEMS_OVERRIDE, 0)
        QN_DECLARE_RW_PROPERTY(bool, isClientUpdateAllowed, setClientUpdateAllowed,
            ALLOW_CLIENT_UPDATE, true)
    QN_END_PROPERTY_STORAGE()
};

#define qnRuntime QnClientRuntimeSettings::instance()

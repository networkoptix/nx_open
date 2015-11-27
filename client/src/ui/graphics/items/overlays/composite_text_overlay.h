
#pragma once

#include <QElapsedTimer>

#include <utils/common/connective.h>
#include <client/client_color_types.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <ui/customization/customized.h>
#include <ui/graphics/items/overlays/text_overlay_widget.h>

class QnWorkbenchNavigator;

/// @brief Class manages bookmarks and texts that are related to owners' media widget
/// TODO: #ynikitenkov Refactor it. Should be separated to complex overlay and distinct single watcher (for data gathering)
class QnCompositeTextOverlay : public Connective< Customized<QnTextOverlayWidget> >
{
public:
    enum Mode
    {
        kUndefinedMode
        , kTextOutputMode
        , kBookmarksMode
    };

private:
    Q_OBJECT 

    Q_PROPERTY(QnCompositeTextOverlayColors colors READ colors WRITE setColors NOTIFY colorsChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

    typedef Connective< Customized<QnTextOverlayWidget> > base_type;

public:

    typedef std::function<qint64 ()> GetUtcCurrentTimeMsFunc;

    QnCompositeTextOverlay(const QnVirtualCameraResourcePtr &camera
        , QnWorkbenchNavigator *navigator
        , const GetUtcCurrentTimeMsFunc &getCurrentUtcTimeMs
        , QGraphicsWidget *parent = nullptr);

    virtual ~QnCompositeTextOverlay();

    void setMode(Mode mode);

    Mode mode() const;

    QnCompositeTextOverlayColors colors() const;

    void setColors(const QnCompositeTextOverlayColors &colors);

private:
    typedef QPair<qint64, QnOverlayTextItemData> InternalData;
    typedef QHash<QnUuid, InternalData> InternalDataHash;

    void addModeData(Mode mode
        , const QnOverlayTextItemData &data);

    void removeModeData(Mode mode
        , const QnUuid &id);

    void setModeData(Mode mode
        , const InternalDataHash &data);

    void resetModeData(Mode mode);

    InternalDataHash::Iterator findModeData(Mode mode
        , const QnUuid &id);

    void initTextMode();

    void initBookmarksMode();

    void at_modeChanged();

    void updateBookmarks();
    
    void updateBookmarksFilter();

    QnOverlayTextItemDataList removeOutdatedItems(Mode mode);

    InternalDataHash makeTextItemData(const QnCameraBookmarkList &bookmarks
        , const QnBookmarkColors &colors);

signals:
    void colorsChanged();

    void modeChanged();

private:
    typedef QHash<Mode, InternalDataHash> ModeInternalDataHash;
    typedef QScopedPointer<QTimer> TimerPtr;

    const QnVirtualCameraResourcePtr m_camera;
    const GetUtcCurrentTimeMsFunc m_getUtcCurrentTimeMs;

    QElapsedTimer m_counter;
    QnWorkbenchNavigator * const m_navigator;

    Mode m_currentMode;
    ModeInternalDataHash m_data;

    QnCameraBookmarksQueryPtr m_bookmarksQuery;

    TimerPtr m_updateQueryFilterTimer;
    TimerPtr m_updateBookmarksTimer;

    QnCompositeTextOverlayColors m_colors;
};

#pragma once

#include <QElapsedTimer>

#include <utils/common/connective.h>
#include <client/client_color_types.h>
#include <camera/camera_bookmarks_manager_fwd.h>
#include <ui/graphics/items/overlays/text_overlay_widget.h>

class QnWorkbenchNavigator;

class QnCompositeTextOverlay : public Connective<QnTextOverlayWidget>
{
    Q_OBJECT 

    Q_PROPERTY(QnMediaResourceWidgetColors colors READ colors WRITE setColors NOTIFY colorsChanged)

    typedef Connective<QnTextOverlayWidget> base_type;
public:

    enum Mode
    {
        kUndefinedMode
        , kTextAlaramsMode
        , kBookmarksMode
    };

    typedef std::function<qint64 ()> GetUtcCurrentTimeMsFunc;

    QnCompositeTextOverlay(const QnVirtualCameraResourcePtr &camera
        , QnWorkbenchNavigator *navigator
        , const GetUtcCurrentTimeMsFunc &getCurrentUtcTimeMs
        , QGraphicsWidget *parent = nullptr);

    virtual ~QnCompositeTextOverlay();

    void setMode(Mode mode);

private:
    typedef QPair<qint64, QnOverlayTextItemData> InnerData;
    typedef QList<InnerData> InnerDataContainer;

    void addModeData(Mode mode
        , const QnOverlayTextItemData &data);

    void removeModeData(Mode mode
        , const QnUuid &id);

    void setModeData(Mode mode
        , const InnerDataContainer &data);

    void resetModeData(Mode mode);

    InnerDataContainer::Iterator findModeData(Mode mode
        , const QnUuid &id);

    void initTextMode();

    void initBookmarksMode();

    QnMediaResourceWidgetColors colors() const;

    void setColors(const QnMediaResourceWidgetColors &colors);

    void currentModeChanged(Mode prev
        , Mode current);

    void updateBookmarks();
    
    void updateBookmarksFilter();

    QnOverlayTextItemDataList filterModeData(Mode mode);

    InnerDataContainer makeTextItemData(const QnCameraBookmarkList &bookmarks
        , const QnBookmarkColors &colors);

signals:
    void colorsChanged();

private:
    typedef QHash<Mode, InnerDataContainer> ModeDataContainer;
    typedef QScopedPointer<QTimer> TimerPtr;

    const QnVirtualCameraResourcePtr m_camera;
    const GetUtcCurrentTimeMsFunc m_getUtcCurrentTimeMs;

    QElapsedTimer m_counter;
    QnWorkbenchNavigator * const m_navigator;

    Mode m_currentMode;
    ModeDataContainer m_data;

    QnCameraBookmarksQueryPtr m_bookmarksQuery;

    TimerPtr m_updateQueryFilterTimer;
    TimerPtr m_updateBookmarksTimer;

    QnMediaResourceWidgetColors m_colors;
};
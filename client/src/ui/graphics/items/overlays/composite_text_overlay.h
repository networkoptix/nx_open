
#pragma once

#include <ui/graphics/items/overlays/text_overlay_widget.h>

class QnCompositeTextOverlay : public QnTextOverlayWidget
{
public:

    enum Mode
    {
        kUndefinedMode
        , kTextAlaramsMode
        , kBookmarksMode
    };

    QnCompositeTextOverlay(QGraphicsWidget *parent = nullptr);

    virtual ~QnCompositeTextOverlay();

    void init(const QnVirtualCameraResourcePtr &camera);

    void setMode(Mode mode);

    void addModeData(Mode mode
        , const QnOverlayTextItemData &data);

    void removeModeData(Mode mode
        , const QnUuid &id);

    void setModeData(Mode mode
        , const QnOverlayTextItemDataList &data);

    void resetModeData(Mode mode);

private:
    typedef QHash<Mode, QnOverlayTextItemDataList> ModeDataContainer;

    QnOverlayTextItemDataList::Iterator findModeData(Mode mode
        , const QnUuid &id);

    void initTextMode(const QnVirtualCameraResourcePtr &camera);

private:
    Mode m_currentMode;
    ModeDataContainer m_data;
};
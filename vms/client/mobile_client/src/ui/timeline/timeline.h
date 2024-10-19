// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDateTime>
#include <QtGui/QColor>
#include <QtQuick/QQuickItem>

#include <common/common_globals.h>
#include <recording/time_period_list.h>

Q_MOC_INCLUDE("nx/vms/client/core/media/chunk_provider.h")

class QnTimelinePrivate;
class QSGGeometryNode;

namespace nx::vms::client::core { class ChunkProvider; }

class QnTimeline: public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(qint64 defaultWindowSize READ defaultWindowSize CONSTANT)
    Q_PROPERTY(qint64 windowSize READ windowSize WRITE setWindowSize NOTIFY windowSizeChanged)
    Q_PROPERTY(qint64 windowStart READ windowStart NOTIFY windowStartChanged)
    Q_PROPERTY(qint64 windowEnd READ windowEnd NOTIFY windowEndChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 startBound READ startBound WRITE setStartBound NOTIFY startBoundChanged)
    Q_PROPERTY(bool stickToEnd READ stickToEnd WRITE setStickToEnd NOTIFY stickToEndChanged)
    Q_PROPERTY(bool autoReturnToBounds
        READ isAutoReturnToBoundsEnabled WRITE setAutoReturnToBoundsEnabled
        NOTIFY autoReturnToBoundsEnabledChanged)
    Q_PROPERTY(int displayOffset
        READ displayOffset WRITE setDisplayOffset NOTIFY displayOffsetChanged)

    Q_PROPERTY(QLocale locale READ locale WRITE setLocale
        NOTIFY localeChanged)

    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor NOTIFY textColorChanged)
    Q_PROPERTY(QColor chunkBarColor
        READ chunkBarColor WRITE setChunkBarColor NOTIFY chunkBarColorChanged)

    Q_PROPERTY(QColor chunkColor READ chunkColor WRITE setChunkColor NOTIFY chunkColorChanged)
    Q_PROPERTY(QColor loadingChunkColor
        READ loadingChunkColor
        WRITE setLoadingChunkColor
        NOTIFY loadingChunkColorChanged)
    Q_PROPERTY(QColor motionModeChunkColor
        READ motionModeChunkColor
        WRITE setMotionModeChunkColor
        NOTIFY motionModeChunkColorChanged)

    Q_PROPERTY(QColor motionColor
        READ motionColor
        WRITE setMotionColor
        NOTIFY motionColorChanged)
    Q_PROPERTY(QColor motionLoadingColor
        READ motionLoadingColor
        WRITE setMotionLoadingColor
        NOTIFY motionLoadingColorChanged)

    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)

    Q_PROPERTY(QColor activeLiveColorLight READ activeLiveColorLight
        WRITE setActiveLiveColorLight NOTIFY activeLiveColorLightChanged)
    Q_PROPERTY(QColor activeLiveColorDark READ activeLiveColorDark
        WRITE setActiveLiveColorDark NOTIFY activeLiveColorDarkChanged)

    Q_PROPERTY(QColor inactiveLiveColorLight READ inactiveLiveColorLight
        WRITE setInactiveLiveColorLight NOTIFY inactiveLiveColorLightChanged)
    Q_PROPERTY(QColor inactiveLiveColorDark READ inactiveLiveColorDark
        WRITE setInactiveLiveColorDark NOTIFY inactiveLiveColorDarkChanged)

    Q_PROPERTY(int chunkBarHeight
        READ chunkBarHeight WRITE setChunkBarHeight NOTIFY chunkBarHeightChanged)
    Q_PROPERTY(int textY READ textY WRITE setTextY NOTIFY textYChanged)

    Q_PROPERTY(nx::vms::client::core::ChunkProvider* chunkProvider
        READ chunkProvider WRITE setChunkProvider NOTIFY chunkProviderChanged)

    Q_PROPERTY(bool motionSearchMode
        WRITE setMotionSearchMode
        READ motionSearchMode
        NOTIFY motionSearchModeChanged)

    Q_PROPERTY(bool changingMotionRoi
        READ changingMotionRoi
        WRITE setChangingMotionRoi
        NOTIFY changingMotionRoiChanged)

    Q_PROPERTY(qint64 rightChunksOffsetMs
        READ rightChunksOffsetMs
        WRITE setRightChunksOffsetMs
        NOTIFY rightChunksOffsetChanged)

public:
    explicit QnTimeline(QQuickItem* parent = nullptr);
    virtual ~QnTimeline() override;

    virtual QSGNode* updatePaintNode(
        QSGNode* node, UpdatePaintNodeData* updatePaintNodeData) override;

    virtual void releaseResources() override;

    QColor textColor() const;
    void setTextColor(const QColor& color);

    QFont font() const;
    void setFont(const QFont& font);

    QColor chunkColor() const;
    void setChunkColor(const QColor& color);

    QColor loadingChunkColor() const;
    void setLoadingChunkColor(const QColor& color);

    QColor motionModeChunkColor() const;
    void setMotionModeChunkColor(const QColor& color);

    QColor motionColor() const;
    void setMotionColor(const QColor& color);

    QColor motionLoadingColor() const;
    void setMotionLoadingColor(const QColor& color);

    QColor chunkBarColor() const;
    void setChunkBarColor(const QColor& color);

    QColor activeLiveColorLight() const;
    void setActiveLiveColorLight(const QColor& color);

    QColor activeLiveColorDark() const;
    void setActiveLiveColorDark(const QColor& color);

    QColor inactiveLiveColorLight() const;
    void setInactiveLiveColorLight(const QColor& color);

    QColor inactiveLiveColorDark() const;
    void setInactiveLiveColorDark(const QColor& color);

    int chunkBarHeight() const;
    void setChunkBarHeight(int chunkBarHeight);

    int textY() const;
    void setTextY(int textY);

    qint64 defaultWindowSize() const;

    qint64 windowStart() const;
    qint64 windowEnd() const;

    qint64 windowSize() const;
    void setWindowSize(qint64 windowSize);

    qint64 position() const;
    void setPosition(qint64 position);
    Q_INVOKABLE void setPositionImmediately(qint64 position);

    bool motionSearchMode() const;
    void setMotionSearchMode(bool value);

    bool changingMotionRoi() const;
    void setChangingMotionRoi(bool value);

    QnTimePeriodList timePeriods(Qn::TimePeriodContent type) const;
    void setTimePeriods(Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods);

    bool stickToEnd() const;
    void setStickToEnd(bool stickToEnd);

    qint64 startBound() const;
    void setStartBound(qint64 startBound);

    bool isAutoReturnToBoundsEnabled() const;
    void setAutoReturnToBoundsEnabled(bool enabled);

    int displayOffset() const;
    void setDisplayOffset(int value);

    void setLocale(const QLocale& locale);
    QLocale locale() const;

    qint64 rightChunksOffsetMs() const;
    void setRightChunksOffsetMs(qint64 value);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();

    Q_INVOKABLE void startZoom(qreal scale);
    Q_INVOKABLE void updateZoom(qreal scale);
    Q_INVOKABLE void finishZoom(qreal scale);

    Q_INVOKABLE void startDrag(int x);
    Q_INVOKABLE void updateDrag(int x);
    Q_INVOKABLE void finishDrag(int x);

    Q_INVOKABLE void clearCorrection();
    Q_INVOKABLE void correctPosition(qint64 position);

    Q_INVOKABLE qint64 positionAtX(qreal x) const;

    nx::vms::client::core::ChunkProvider* chunkProvider() const;
    void setChunkProvider(nx::vms::client::core::ChunkProvider* chunkProvider);

signals:
    void rightChunksOffsetChanged();
    void zoomLevelChanged();
    void lowerTextOpacityChanged();
    void windowSizeChanged();
    void windowStartChanged();
    void windowEndChanged();
    void positionChanged();
    void stickToEndChanged();
    void startBoundChanged();
    void autoReturnToBoundsEnabledChanged();
    void displayOffsetChanged();
    void localeChanged();

    void textColorChanged();
    void chunkColorChanged();
    void loadingChunkColorChanged();
    void motionModeChunkColorChanged();
    void motionColorChanged();
    void motionLoadingColorChanged();
    void chunkBarColorChanged();
    void fontChanged();

    void activeLiveColorDarkChanged();
    void activeLiveColorLightChanged();
    void inactiveLiveColorDarkChanged();
    void inactiveLiveColorLightChanged();

    void chunkBarHeightChanged();
    void textYChanged();

    void chunkProviderChanged();

    void moveFinished();
    void motionSearchModeChanged();
    void changingMotionRoiChanged();

private:
    QSGNode* updateTextNode(QSGNode* textRootNode);
    QSGGeometryNode* updateChunksNode(QSGGeometryNode* chunksNode);

private:
    QScopedPointer<QnTimelinePrivate> d;
};

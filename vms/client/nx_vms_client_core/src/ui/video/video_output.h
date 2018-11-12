#pragma once

#include <QtCore/QRect>
#include <QtQuick/QQuickItem>

namespace nx {
namespace media {

class Player;

} // namespace media
} // namespace nx

class QnVideoOutputPrivate;

class QnVideoOutput: public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(nx::media::Player* player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(QRectF sourceRect READ sourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY contentRectChanged)

    using base_type = QQuickItem;

public:
    enum FillMode
    {
        Stretch = Qt::IgnoreAspectRatio,
        PreserveAspectFit = Qt::KeepAspectRatio,
        PreserveAspectCrop = Qt::KeepAspectRatioByExpanding
    };
    Q_ENUM(FillMode)

    QnVideoOutput(QQuickItem* parent = nullptr);
    ~QnVideoOutput();

    nx::media::Player* player() const;
    Q_INVOKABLE void setPlayer(nx::media::Player* player, int channel = 0);

    FillMode fillMode() const;
    void setFillMode(FillMode mode);

    QRectF sourceRect() const;
    QRectF contentRect() const;

    void updateNativeSize();

    Q_INVOKABLE void clear();

signals:
    void playerChanged();
    void fillModeChanged(QnVideoOutput::FillMode);
    void sourceRectChanged();
    void contentRectChanged();

protected:
    QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*);
    void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry);

private:
    QScopedPointer<QnVideoOutputPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnVideoOutput)
};

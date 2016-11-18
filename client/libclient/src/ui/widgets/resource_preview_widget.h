#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

class QLabel;
class QStackedWidget;
class QnBusyIndicatorWidget;
class QnCameraThumbnailManager;

class QnResourcePreviewWidget : public Connective<QWidget>
{
    Q_OBJECT
    typedef Connective<QWidget> base_type;

public:
    explicit QnResourcePreviewWidget(QWidget* parent = nullptr);
    virtual ~QnResourcePreviewWidget();

    const QnResourcePtr& targetResource() const;
    void setTargetResource(const QnResourcePtr& target);

    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize& size);

    QnBusyIndicatorWidget* busyIndicator() const;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void changeEvent(QEvent* event) override;
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

private:
    void retranslateUi();

private:
    QScopedPointer<QnCameraThumbnailManager> m_thumbnailManager;
    QnResourcePtr m_target;
    mutable QSize m_cachedSizeHint;
    QSize m_resolutionHint;
    qreal m_aspectRatio; //TODO: Refactor to QnAspectRatio
    QLabel* m_preview;
    QLabel* m_placeholder;
    QnBusyIndicatorWidget* m_indicator;
    QStackedWidget* m_pages;
    int m_status;
};

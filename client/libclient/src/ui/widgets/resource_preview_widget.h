#pragma once

#include <QtWidgets/QWidget>

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

    QnUuid targetResource() const;
    void setTargetResource(const QnUuid& target);

    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize& size);

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
    QnUuid m_target;
    mutable QSize m_cachedSizeHint;
    QSize m_resolutionHint;
    QLabel* m_preview;
    QLabel* m_placeholder;
    QWidget* m_indicator;
    QStackedWidget* m_pages;
    int m_status;
};

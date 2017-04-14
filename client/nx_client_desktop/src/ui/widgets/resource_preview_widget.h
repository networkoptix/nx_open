#pragma once

#include <client/client_globals_fwd.h>

#include <core/resource/resource_fwd.h>

#include <utils/common/connective.h>

class QLabel;
class QStackedWidget;
class QnBusyIndicatorWidget;
class QnAutoscaledPlainText;
class QnImageProvider;

class QnResourcePreviewWidget : public Connective<QWidget>
{
    Q_OBJECT
    typedef Connective<QWidget> base_type;

public:
    explicit QnResourcePreviewWidget(QWidget* parent = nullptr);
    virtual ~QnResourcePreviewWidget();

    QnImageProvider* imageProvider() const;
    void setImageProvider(QnImageProvider* provider);

    QnBusyIndicatorWidget* busyIndicator() const;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void changeEvent(QEvent* event) override;
    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

private:
    void retranslateUi();
    void invalidateGeometry();

    void updateThumbnailStatus(Qn::ThumbnailStatus status);
    void updateThumbnailImage(const QImage& image);


private:
    mutable QSize m_cachedSizeHint;
    QLabel* m_preview = nullptr;
    QnAutoscaledPlainText* m_placeholder = nullptr;
    QnBusyIndicatorWidget* m_indicator = nullptr;
    QStackedWidget* m_pages = nullptr;
    QPointer<QnImageProvider> m_imageProvider;
};

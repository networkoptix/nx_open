#pragma once

#include <QtCore/QEvent>

#include <ui/common/widget_limits_backup.h>
#include <utils/common/object_companion.h>

/*
* This helper class improves minimum/maximum size calculation of QTabWidget and QStackedWidget.
*/
template<class Pages>
class QnPageSizeAdjuster: public QObject
{
public:
    /*
    * Installs page size adjuster to specified paged widget.
    *  It will adjust size limits at appropriate moments.
    *
    * Size policy of invisible pages will be set to { Ignored, Ignored },
    *  size policy of current page will be set to visiblePagePolicy.
    *
    * If resizeToVisible is true, size limits will be computed by current page only,
    *  otherwise size limits will be computed by all pages.
    *
    * Optional extraHandler will be called after every adjustment of size limits,
    *  it can be used for extra layout processing (e.g. dialog shrinking to content).
    */
    static QnPageSizeAdjuster* install(
        Pages* pages,
        QSizePolicy visiblePagePolicy,
        bool resizeToVisible,
        std::function<void()> extraHandler = std::function<void()>())
    {
        auto adjuster = new QnPageSizeAdjuster(
            pages,
            visiblePagePolicy,
            resizeToVisible,
            extraHandler);

        QnObjectCompanionManager::attach(pages, adjuster, "resizePagesToContents");
        return adjuster;
    }

private:
    QnPageSizeAdjuster(
        Pages* pages,
        QSizePolicy visiblePagePolicy,
        bool resizeToVisible,
        std::function<void()> extraHandler)
        :
        QObject(pages),
        m_pages(pages),
        m_visiblePagePolicy(visiblePagePolicy),
        m_resizeToVisible(resizeToVisible),
        m_extraHandler(extraHandler)
    {
        connect(m_pages, &Pages::currentChanged, this, &QnPageSizeAdjuster::updatePageSizes);
        m_pages->installEventFilter(this);

        updatePageSizes(m_pages->currentIndex());
    }

    void updatePageSizes(int current)
    {
        m_widgetLimitsBackup.restore();

        if (current < 0)
            return;

        QSize minimumSize(0, 0);
        QSize maximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

        for (int i = 0; i < m_pages->count(); ++i)
        {
            auto page = m_pages->widget(i);

            static const QSizePolicy kIgnore(QSizePolicy::Ignored, QSizePolicy::Ignored);
            page->setSizePolicy(i == current ? m_visiblePagePolicy : kIgnore);

            if (m_resizeToVisible)
                continue;

            maximumSize = maximumSize.boundedTo(page->maximumSize());

            minimumSize = minimumSize.expandedTo(page->minimumSizeHint())
                                     .expandedTo(page->minimumSize());

            if (page->hasHeightForWidth())
            {
                minimumSize.setHeight(qMax(minimumSize.height(), page->heightForWidth(
                    page->parentWidget()->contentsRect().width())));
            }
        }

        if (!m_resizeToVisible)
        {
            auto currentPage = m_pages->currentWidget();
            m_widgetLimitsBackup.backup(currentPage);
            currentPage->setMinimumSize(minimumSize);
            currentPage->setMaximumSize(maximumSize.expandedTo(minimumSize));
        }

        if (m_extraHandler)
            m_extraHandler();
    }

public:
    virtual ~QnPageSizeAdjuster()
    {
        m_widgetLimitsBackup.restore();
    }

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_pages)
            return false;

        switch (event->type())
        {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::LayoutRequest:
                updatePageSizes(m_pages->currentIndex());
                break;

            default:
                break;
        }

        return false;
    }

private:
    Pages* m_pages;
    QSizePolicy m_visiblePagePolicy;
    bool m_resizeToVisible;
    std::function<void()> m_extraHandler;
    QnWidgetLimitsBackup m_widgetLimitsBackup;
};


using QnStackedWidgetPageSizeAdjuster = QnPageSizeAdjuster<QStackedWidget>;
using QnTabWidgetPageSizeAdjuster = QnPageSizeAdjuster<QTabWidget>;

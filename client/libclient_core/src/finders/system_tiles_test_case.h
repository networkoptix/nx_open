#pragma once

#include <functional>
#include <QtCore/QObject>

class QnTestSystemsFinder;

enum class QnTileTest
{
    First,

    ChangeWeightOnCollapse = First,
    ChangeVersion,
    MaximizeAppOnCollapse,
    SwitchPage,

    Count
};

class QnSystemTilesTestCase : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnSystemTilesTestCase(
        QnTestSystemsFinder* finder,
        QObject* parent);

    ~QnSystemTilesTestCase() = default;

    typedef std::function<void (QnTileTest id)> CompletionHandler;
    void startTest(
        QnTileTest test,
        int delay = 0,
        CompletionHandler completionHandler = CompletionHandler());

    void runTestSequence(
        QnTileTest current,
        int delay = 0);

signals:
    void openTile(const QString& systemId);
    void collapseExpandedTile();

    void makeFullscreen();
    void restoreApp();

    void switchPage(int pageNumber);

    void messageChanged(const QString& message);

private:
    void changeWeightsTest(CompletionHandler completionHandler);

    void maximizeTest(CompletionHandler completionHandler);

    void versionChangeTest(CompletionHandler completionHandler);

    void switchPageTest(CompletionHandler completionHandler);

    void showAutohideMessage(
        const QString& message,
        qint64 hideDelay);
private:
    QnTestSystemsFinder* const m_finder;
    QnTileTest m_currentTileTest = QnTileTest::Count;
};

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_profiler.h"

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtWidgets/QApplication>

namespace nx::vms::client::desktop {

void WidgetProfiler::saveWidgetsReport(const QString& filePath,
    const QWidget* parent /*= nullptr*/)
{
    QFile file(filePath);
    file.open(QFile::WriteOnly | QFile::Truncate);
    if (!file.isOpen())
        return;
    QTextStream textStream(&file);
    textStream.setCodec("UTF-8");
    textStream << "WIDGET COUNT BY TYPE" << Qt::endl << Qt::endl << getWidgetStatistics() << Qt::endl;
    textStream << "WIDGETS HIERARCHY DUMP" << Qt::endl << Qt::endl << getWidgetHierarchy(parent);
    textStream.flush();
}

namespace {

QStringList getWidgetHierarchyLines(const QWidget* rootWidget)
{
    QStringList lines;
    if (!rootWidget)
        return lines;

    QString widgetHierarchyLine(rootWidget->metaObject()->className());
    if (!rootWidget->objectName().isEmpty())
        widgetHierarchyLine.append(QString(" (%1)").arg(rootWidget->objectName()));
    lines.append(widgetHierarchyLine);

    const auto children =
        rootWidget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (int i = 0; i < children.count(); ++i)
    {
        constexpr QChar boxUpAndRight(0x2514);
        constexpr QChar boxVertical(0x2502);
        constexpr QChar boxVerticalRight(0x251C);
        const bool isLastChild = children.count() - 1 == i;
        const auto firstIndent =
            QString("%1 ").arg(isLastChild ? boxUpAndRight : boxVerticalRight);
        const auto nextIndent =
            QString("%1 ").arg(isLastChild ? QChar(' ') : boxVertical);

        auto childHierarchyLines = getWidgetHierarchyLines(children.at(i));
        lines.append(childHierarchyLines.takeFirst().prepend(firstIndent));
        for (const auto& childHierarchyLine : childHierarchyLines)
            lines.append(nextIndent + childHierarchyLine);
    }
    return lines;
}

} // namespace

QString WidgetProfiler::getWidgetHierarchy(const QWidget* parent /*= nullptr*/)
{
    const auto allWidgets = QApplication::allWidgets();
    QWidgetList listedWidgets;
    std::copy_if(allWidgets.cbegin(), allWidgets.cend(), std::back_inserter(listedWidgets),
        [parent](const QWidget* widget) { return widget->parentWidget() == parent; });

    QString result;
    QTextStream textStream(&result);
    for (const auto& listedWidget: listedWidgets)
    {
        for (const auto& hierarchyLine: getWidgetHierarchyLines(listedWidget))
            textStream << hierarchyLine << Qt::endl;
        textStream << Qt::endl;
    }
    textStream.flush();
    return result;
}

QString WidgetProfiler::getWidgetStatistics()
{
    QHash<QString, int> countByClassName;
    int longestClassName = 0;
    for (const auto& widget: QApplication::allWidgets())
    {
        const QString className(widget->metaObject()->className());
        longestClassName = std::max(longestClassName, className.size());
        if (!countByClassName.contains(className))
            countByClassName.insert(className, 1);
        else
            ++countByClassName[className];
    }

    QVector<QPair<QString, int>> sortedByCount;
    for (auto i = countByClassName.cbegin(); i != countByClassName.cend(); ++i)
        sortedByCount.append({i.key(), i.value()});
    std::sort(sortedByCount.begin(), sortedByCount.end(),
        [](const auto& l, const auto& r) { return l.second > r.second; });

    return std::accumulate(sortedByCount.cbegin(), sortedByCount.cend(), QString(),
        [longestClassName](const auto& accumulated, const auto& pair)
        {
            return accumulated + QString("%1 %2\n")
                .arg(pair.first, -longestClassName, ' ').arg(pair.second);
        });
}

} // namespace nx::vms::client::desktop

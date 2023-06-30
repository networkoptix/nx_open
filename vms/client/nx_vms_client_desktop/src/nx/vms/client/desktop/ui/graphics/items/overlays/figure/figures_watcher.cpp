// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "figures_watcher.h"

#include "box.h"
#include "polyline.h"
#include "polygon.h"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <client/client_module.h>
#include <nx/vms/client/core/analytics/analytics_settings_multi_listener.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace {

using namespace nx::vms::client::desktop::figure;

using FigureTypeHash = QHash<FigureId, FigureType>;
using FigureTypesByEngine = QHash<EngineId, FigureTypeHash>;

using FigureTextsHash = QHash<FigureId, QString>;
using FigureTextsByEngine = QHash<EngineId, FigureInfoHash>;

FigureTypeHash extractSettings(const QJsonObject& settings)
{
    QJsonDocument doc(settings);
    QString strJson(doc.toJson(QJsonDocument::Compact));
    static const QHash<QString, FigureType> kFigureTypes{
        {"LineFigure", FigureType::polyline},
        {"PolylineFigure", FigureType::polyline},
        {"BoxFigure", FigureType::box},
        {"PolygonFigure", FigureType::polygon},
        {"PointFigure", FigureType::point},
    };

    const auto type = kFigureTypes.value(
        settings.value(QStringLiteral("type")).toString(), FigureType::invalid);

    FigureTypeHash result;
    if (type != FigureType::invalid)
    {
        const auto name = settings.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            result.insert(name, type);
        return result;
    }

    // TODO: Recursive parser is not 100% correct. Protect it against injection of invalid items
    // and sections.
    const auto items = settings.value(QStringLiteral("items")).toArray();
    if (!items.isEmpty())
    {
        for (const auto& value: items)
        {
            if (value.isObject())
                result.insert(extractSettings(value.toObject()));
        }
    }

    const auto sections = settings.value(QStringLiteral("sections")).toArray();
    if (!sections.isEmpty())
    {
        for (const auto& value: sections)
        {
            if (value.isObject())
                result.insert(extractSettings(value.toObject()));
        }
    }

    return result;
}

Polyline::Direction direction(const QJsonObject& figure)
{
    const auto& direction = figure.value(QStringLiteral("direction")).toString();

    if (direction == QStringLiteral("left"))
        return Polyline::Direction::left;

    if (direction == QStringLiteral("right"))
        return Polyline::Direction::right;

    return Polyline::Direction::both;
}

FigureInfo extractFigure(FigureType type, const QJsonObject& data)
{
    static const auto kDefaultRoiColor =
        nx::vms::client::core::colorTheme()->colors("roi.palette")[0].name();

    const bool visible = data.value(QStringLiteral("showOnCamera")).toBool();
    if (!visible)
        return FigureInfo();

    const QJsonObject& figure = data.value(QStringLiteral("figure")).toObject();
    if (figure.isEmpty())
        return FigureInfo();

    const QColor color = figure.value(QStringLiteral("color")).toString(kDefaultRoiColor);

    const QString labelText = data.value(QStringLiteral("label")).toString();

    Points points;
    for (const auto point: figure.value(QStringLiteral("points")).toArray())
    {
        if (!point.isArray())
            continue;

        const auto& coordinates = point.toArray();
        if (coordinates.size() == 2)
            points.append({coordinates[0].toDouble(), coordinates[1].toDouble()});
    }

    const auto result =
        [=]()->FigureInfo
        {
            // Typedefs prevents "ambiguous symbol" error on Windows build.
            using PolylineType = nx::vms::client::desktop::figure::Polyline;
            using PolygonType = nx::vms::client::desktop::figure::Polygon;
            using BoxType = nx::vms::client::desktop::figure::Box;
            switch (type)
            {
                case FigureType::polyline:
                    return {make<PolylineType>(points, color, direction(figure)), labelText};
                case FigureType::box:
                    return {make<BoxType>(points, color), labelText};
                case FigureType::polygon:
                    return {make<PolygonType>(points, color), labelText};
                default:
                    return FigureInfo();
            }
        }();

    return (result.figure && result.figure->isValid()) ? result : FigureInfo();
}

} // namespace

namespace nx::vms::client::desktop::figure {

struct RoiFiguresWatcher::Private
{
    RoiFiguresWatcher* const q;

    QScopedPointer<core::AnalyticsSettingsMultiListener> settingsListener;
    FigureInfosByEngine figureInfosByEngine;

    Private(RoiFiguresWatcher* q);

    void handleEngineDataUpdated(
        const EngineId& engineId,
        const core::DeviceAgentData& data);
};

RoiFiguresWatcher::Private::Private(RoiFiguresWatcher* q):
    q(q)
{
}

void RoiFiguresWatcher::Private::handleEngineDataUpdated(
    const EngineId& engineId,
    const core::DeviceAgentData& data)
{
    const auto settings = extractSettings(data.model);

    QSet<FigureId> current;
    FigureInfoHash figures;
    for (auto it = settings.cbegin(); it != settings.cend(); ++it)
    {
        const auto& id = it.key();
        const auto figureType = it.value();
        const QJsonObject& figureData = data.values.value(id).toObject();
        if (figureData.isEmpty())
            continue;
        const auto figureInfo = extractFigure(figureType, figureData);
        if (figureInfo.figure)
        {
            current.insert(id);
            figures.insert(id, figureInfo);
        }
    }

    auto& oldFigures = figureInfosByEngine[engineId];
    const auto& oldKeysList =  oldFigures.keys();
    const QSet<FigureId> old(oldKeysList.begin(), oldKeysList.end());

    const auto added = current - old;
    const auto removed = old - current;

    auto remaining = old & current;
    std::vector<FigureId> updated;
    for (const auto& id: remaining)
    {
        const auto& oldFigure = oldFigures.value(id);
        const auto& newFigure = figures.value(id);
        if (*oldFigure.figure != *newFigure.figure)
            updated.push_back(id);
    }

    figureInfosByEngine[engineId] = figures;

    for (const auto& removeFigureId: removed)
        emit q->figureRemoved(engineId, removeFigureId);
    for (const auto& addedFigureId: added)
        emit q->figureAdded(engineId, addedFigureId);
    for (const auto& updatedFigureId: updated)
        emit q->figureChanged(engineId, updatedFigureId);
}

//-------------------------------------------------------------------------------------------------

FiguresWatcherPtr RoiFiguresWatcher::create(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
{
    return FiguresWatcherPtr(new RoiFiguresWatcher(camera, parent));
}

RoiFiguresWatcher::RoiFiguresWatcher(
    const QnVirtualCameraResourcePtr& camera,
    QObject* parent):
    base_type(parent),
    d(new Private(this))
{
    if (!camera)
        return;

    d->settingsListener.reset(new core::AnalyticsSettingsMultiListener(
        qnClientModule->analyticsSettingsManager(),
        camera,
        core::AnalyticsSettingsMultiListener::ListenPolicy::enabledEngines));

    connect(d->settingsListener.get(), &core::AnalyticsSettingsMultiListener::dataChanged, this,
        [this](const EngineId& engineId, const core::DeviceAgentData& data)
        {
            d->handleEngineDataUpdated(engineId, data);
        });

    const auto handleEnginesChanged =
        [this]()
        {
            d->figureInfosByEngine.clear();
            for (const auto& engineId: d->settingsListener->engineIds())
                d->handleEngineDataUpdated(engineId, d->settingsListener->data(engineId));
        };
    connect(d->settingsListener.get(), &core::AnalyticsSettingsMultiListener::enginesChanged,
        this, handleEnginesChanged);

    handleEnginesChanged();
}

RoiFiguresWatcher::~RoiFiguresWatcher()
{
}

FigureInfosByEngine RoiFiguresWatcher::figures() const
{
    FigureInfosByEngine result;
    for (auto it = d->figureInfosByEngine.cbegin(); it != d->figureInfosByEngine.cend(); ++it)
    {
        FigureInfoHash newFigures;
        const auto& oldFigures = it.value();
        for (auto iterInfo = oldFigures.cbegin(); iterInfo != oldFigures.cend(); ++iterInfo)
            newFigures.insert(iterInfo.key(), {iterInfo->figure->clone(), iterInfo->labelText});

        result.insert(it.key(), newFigures);
    }
    return result;
}

FigurePtr RoiFiguresWatcher::figure(const EngineId& engineId, const FigureId& figureId) const
{
    const auto iterInfos = d->figureInfosByEngine.find(engineId);
    if (iterInfos == d->figureInfosByEngine.end())
        return FigurePtr();

    const auto& figureInfos = iterInfos.value();
    const auto iterInfo = figureInfos.find(figureId);
    return (iterInfo != figureInfos.end())
        ? iterInfo->figure->clone()
        : FigurePtr();
}

QString RoiFiguresWatcher::figureLabelText(
    const EngineId& engineId,
    const FigureId& figureId) const
{
    const auto iterInfos = d->figureInfosByEngine.find(engineId);
    if (iterInfos == d->figureInfosByEngine.end())
        return {};

    const auto& figureInfos = iterInfos.value();
    const auto iterInfo = figureInfos.find(figureId);
    return (iterInfo != figureInfos.end())
        ? iterInfo->labelText
        : "";
}

} // namespace nx::vms::client::desktop::figure

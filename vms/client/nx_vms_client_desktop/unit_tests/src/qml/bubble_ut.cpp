// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include <QtCore/QObject>
#include <QtQml/QJSValue>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>

#include <nx/vms/client/core/utils/qml_helpers.h>
#include <nx/vms/client/desktop/utils/qml_property.h>

#include "qml_test_environment.h"

namespace nx::vms::client::desktop {
namespace test {

class BubbleTest: public ::testing::Test
{
public:
    virtual void SetUp() override
    {
        QQmlComponent bubbleComponent(m_env.engine(), ":/qml/Nx/Controls/Bubble.qml",
            QQmlComponent::PreferSynchronous);

        m_bubble.reset(bubbleComponent.create());
        ASSERT_TRUE((bool) m_bubble);

        QQmlComponent contentComponent(m_env.engine());
        contentComponent.setData("import QtQuick 2.0; Item {}", {});

        m_content.reset(qobject_cast<QQuickItem*>(contentComponent.create()));
        ASSERT_TRUE((bool) m_content);

        contentWidth = QmlProperty<qreal>(m_content.get(), "implicitWidth");
        contentWidth.setValue(100);
        contentHeight = QmlProperty<qreal>(m_content.get(), "implicitHeight");
        contentHeight.setValue(50);

        QmlProperty<QQuickItem*>(m_bubble.get(), "contentItem").setValue(m_content.get());

        shadowOffset = QmlProperty<QPointF>(m_bubble.get(), "shadowOffset");
        shadowOffset.setValue({0, 0});
        shadowBlurRadius = QmlProperty<qreal>(m_bubble.get(), "shadowBlurRadius");
        shadowBlurRadius.setValue(0);

        leftPadding = QmlProperty<qreal>(m_bubble.get(), "leftPadding");
        rightPadding = QmlProperty<qreal>(m_bubble.get(), "rightPadding");
        topPadding = QmlProperty<qreal>(m_bubble.get(), "topPadding");
        bottomPadding = QmlProperty<qreal>(m_bubble.get(), "bottomPadding");
        pointerLength = QmlProperty<qreal>(m_bubble.get(), "pointerLength");
    }

    virtual void TearDown() override
    {
        m_bubble.reset();
    }

    struct Result
    {
        Qt::Edge pointerEdge;
        qreal normalizedPointerPos;
        qreal x;
        qreal y;
    };

    void calculateParameters(std::optional<Result>* result, Qt::Orientation orientation,
        const QRectF& targetRect, const QRectF& enclosingRect, qreal minIntersection = 0) const
    {
        const auto params = nx::vms::client::core::invokeQmlMethod<QJSValue>(
            m_bubble.get(),
            "calculateParameters",
            (int) orientation,
            targetRect,
            enclosingRect,
            minIntersection);

        if (params.isUndefined())
        {
            *result = std::nullopt;
            return;
        }

        ASSERT_TRUE(params.isObject());
        ASSERT_TRUE(params.hasOwnProperty("pointerEdge"));
        ASSERT_TRUE(params.hasOwnProperty("normalizedPointerPos"));
        ASSERT_TRUE(params.hasOwnProperty("x"));
        ASSERT_TRUE(params.hasOwnProperty("y"));

        *result = Result{
            (Qt::Edge) params.property("pointerEdge").toInt(),
            params.property("normalizedPointerPos").toNumber(),
            params.property("x").toNumber(),
            params.property("y").toNumber()};
    }

    // Sets required content size, based on specified bubble size (with paddings and pointer,
    // but without shadow) and specified orientation.
    void setSize(qreal width, qreal height, Qt::Orientation forOrientation)
    {
        QSize contentSize(width - leftPadding - rightPadding, height - topPadding - bottomPadding);

        if (forOrientation == Qt::Horizontal)
            contentSize.rwidth() -= pointerLength;
        else
            contentSize.rheight() -= pointerLength;

        contentWidth = contentSize.width();
        contentHeight = contentSize.height();
    }

    QmlProperty<QPointF> shadowOffset;
    QmlProperty<qreal> shadowBlurRadius;

private:
    const QmlTestEnvironment m_env;
    std::unique_ptr<QObject> m_bubble;
    std::unique_ptr<QQuickItem> m_content;

    QmlProperty<qreal> contentWidth;
    QmlProperty<qreal> contentHeight;
    QmlProperty<qreal> leftPadding;
    QmlProperty<qreal> rightPadding;
    QmlProperty<qreal> topPadding;
    QmlProperty<qreal> bottomPadding;
    QmlProperty<qreal> pointerLength;
};

TEST_F(BubbleTest, automaticEdgeSelection)
{
    std::optional<Result> result;
    setSize(100, 100, Qt::Horizontal);

    // Fits only the left side.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(200, 100, 150, 100),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::RightEdge);

    // Fits only the right side.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(50, 100, 150, 100),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::LeftEdge);

    // When the bubble fits both left and right sides, the right side is preferred.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(200, 100, 50, 100),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::LeftEdge);

    setSize(100, 100, Qt::Vertical);

    // Fits only the top side.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 200, 100, 150),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::BottomEdge);

    // Fits only the bottom side.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 50, 100, 150),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::TopEdge);

    // When the bubble fits both top and bottom sides, the top side is preferred.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 200, 100, 50),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::BottomEdge);
}

TEST_F(BubbleTest, calculateOffset)
{
    std::optional<Result> result;

    setSize(200, 60, Qt::Horizontal);

    // Horizontal, centered.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 200, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::LeftEdge);
    ASSERT_EQ(result->normalizedPointerPos, 0.5);
    ASSERT_EQ(result->x, 100.0);
    ASSERT_EQ(result->y, 180.0);

    // Horizontal, shift up.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 380, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::LeftEdge);
    ASSERT_EQ(result->normalizedPointerPos, (390.0 - 340.0) / 60.0);
    ASSERT_EQ(result->x, 100.0);
    ASSERT_EQ(result->y, 340.0);

    // Horizontal, shift down.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(350, 0, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::RightEdge);
    ASSERT_EQ(result->normalizedPointerPos, 10.0 / 60.0);
    ASSERT_EQ(result->x, 150.0);
    ASSERT_EQ(result->y, 0.0);

    setSize(200, 60, Qt::Vertical);

    // Vertical, centered.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 200, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::BottomEdge);
    ASSERT_EQ(result->normalizedPointerPos, 0.5);
    ASSERT_EQ(result->x, 50.0);
    ASSERT_EQ(result->y, 140.0);

    // Vertical, shift right.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(0, 200, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::BottomEdge);
    ASSERT_EQ(result->normalizedPointerPos, 50.0 / 200.0);
    ASSERT_EQ(result->x, 0.0);
    ASSERT_EQ(result->y, 140.0);

    // Vertical, shift left.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(300, 0, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::TopEdge);
    ASSERT_EQ(result->normalizedPointerPos, (350.0 - 200.0) / 200.0);
    ASSERT_EQ(result->x, 200.0);
    ASSERT_EQ(result->y, 20.0);
}

TEST_F(BubbleTest, bubbleDoesntFit)
{
    std::optional<Result> result;
    setSize(100, 100, Qt::Horizontal);

    // Horizontal bubble doesn't fit horizontally.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(50, 100, 350, 100),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_FALSE(result.has_value());

    // Horizontal bubble doesn't fit vertically.
    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(200, 0, 50, 50),
        /*enclosingRect*/ QRectF(0, 0, 400, 99));

    ASSERT_FALSE(result.has_value());

    setSize(100, 100, Qt::Vertical);

    // Vertical bubble doesn't fit vertically.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 50, 100, 350),
        /*enclosingRect*/ QRectF(0, 0, 400, 400));

    ASSERT_FALSE(result.has_value());

    // Vertical bubble doesn't fit horizontally.
    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(0, 200, 100, 220),
        /*enclosingRect*/ QRectF(0, 0, 99, 400));

    ASSERT_FALSE(result.has_value());
}

TEST_F(BubbleTest, targetIntersectionConstraints)
{
    std::optional<Result> result;
    setSize(100, 20, Qt::Horizontal);

    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 100, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 109) //< Vertical intersection with targetRect is 9.
        /*minIntersection will be 10 by default*/);

    ASSERT_FALSE(result.has_value());

    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 100, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 400, 110) //< Vertical intersection with targetRect is 10.
        /*minIntersection will be 10 by default*/);

    ASSERT_TRUE(result.has_value());

    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 89, 100, 20),
        /*enclosingRect*/ QRectF(0, 100, 400, 100) //< Vertical intersection with targetRect is 9.
        /*minIntersection will be 10 by default*/);

    ASSERT_FALSE(result.has_value());

    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 90, 100, 20),
        /*enclosingRect*/ QRectF(0, 100, 400, 100) //< Vertical intersection with targetRect is 10.
        /*minIntersection will be 10 by default*/);

    ASSERT_TRUE(result.has_value());

    setSize(100, 20, Qt::Vertical);

    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 0, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 149, 100) //< Horizontal intersection with targetRect is 49.
        /*minIntersection will be 50 by default*/);

    ASSERT_FALSE(result.has_value());

    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(100, 0, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 150, 100) //< Horizontal intersection with targetRect is 50.
        /*minIntersection will be 50 by default*/);

    ASSERT_TRUE(result.has_value());

    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(49, 0, 100, 20),
        /*enclosingRect*/ QRectF(100, 0, 400, 100) //< Horizontal intersection w/ targetRect is 49.
        /*minIntersection will be 50 by default*/);

    ASSERT_FALSE(result.has_value());

    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(50, 0, 100, 20),
        /*enclosingRect*/ QRectF(100, 0, 400, 100) //< Horizontal intersection w/ targetRect is 50.
        /*minIntersection will be 50 by default*/);

    ASSERT_TRUE(result.has_value());
}

TEST_F(BubbleTest, shadowIsNonConstraining)
{
    std::optional<Result> result;

    shadowBlurRadius = 5;

    setSize(200, 60, Qt::Horizontal);

    calculateParameters(&result,
        Qt::Horizontal,
        /*targetRect*/ QRectF(0, 0, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 300, 60));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::LeftEdge);
    ASSERT_EQ(result->normalizedPointerPos, 10.0 / 60.0);
    ASSERT_EQ(result->x, 100.0);
    ASSERT_EQ(result->y, 0);

    setSize(200, 60, Qt::Vertical);

    calculateParameters(&result,
        Qt::Vertical,
        /*targetRect*/ QRectF(0, 200, 100, 20),
        /*enclosingRect*/ QRectF(0, 0, 200, 400));

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->pointerEdge, Qt::BottomEdge);
    ASSERT_EQ(result->normalizedPointerPos, 50.0 / 200.0);
    ASSERT_EQ(result->x, 0);
    ASSERT_EQ(result->y, 140.0);
}

} // namespace test
} // namespace nx::vms::client::desktop

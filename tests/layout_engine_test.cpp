#include "core/layout_engine.h"

#include <QtTest>

#include <vector>

using Core::LayoutEngine;
using Core::LayoutItem;

class LayoutEngineTest final : public QObject {
    Q_OBJECT

private slots:
    void swapsDifferentSpansWithoutOverlap()
    {
        const std::vector<LayoutItem> items{{"clock", 0, 3}, {"cpu", 5, 2}};
        const auto result = LayoutEngine::drop(items, "clock", 5, 9);

        QVERIFY(result.has_value());
        QCOMPARE(result->at(0).slot, 5);
        QCOMPARE(result->at(1).slot, 0);
        QVERIFY(LayoutEngine::isValid(*result, 9));
    }

    void rejectsMultiCollisionTransaction()
    {
        const std::vector<LayoutItem> items{{"wide", 0, 3}, {"a", 4, 2}, {"b", 6, 2}};
        const auto result = LayoutEngine::drop(items, "wide", 4, 8);

        QVERIFY(!result.has_value());
    }

    void rejectsSwapWhenDisplacedItemDoesNotFitOldInterval()
    {
        const std::vector<LayoutItem> items{{"small", 0, 2}, {"wide", 3, 3}};

        QVERIFY(!LayoutEngine::drop(items, "small", 3, 8).has_value());
    }

    void clampsDraggedItemToAvailableSlots()
    {
        const std::vector<LayoutItem> items{{"clock", 0, 3}};
        const auto result = LayoutEngine::drop(items, "clock", 99, 7);

        QVERIFY(result.has_value());
        QCOMPARE(result->at(0).slot, 4);
    }

    void normalizesAfterScreenShrink()
    {
        const std::vector<LayoutItem> items{{"a", 8, 2}, {"b", 1, 3}};
        const auto result = LayoutEngine::normalize(items, 7);

        QVERIFY(LayoutEngine::isValid(result, 7));
        QCOMPARE(result.size(), items.size());
    }

    void returnsEmptyNormalizationWhenCapacityIsInsufficient()
    {
        const std::vector<LayoutItem> items{{"a", 0, 4}, {"b", 4, 4}};

        QVERIFY(LayoutEngine::normalize(items, 7).empty());
    }

    void normalizesUsingStableNearestLowerPreference()
    {
        const std::vector<LayoutItem> items{{"first", 2, 2}, {"second", 2, 1}};
        const auto result = LayoutEngine::normalize(items, 5);

        QCOMPARE(result.at(0).slot, 2);
        QCOMPARE(result.at(1).slot, 1);
    }

    void normalizesAllItemsWhenTotalCapacityFits()
    {
        const std::vector<LayoutItem> items{{"small", 2, 1}, {"wide", 0, 3}};
        const auto result = LayoutEngine::normalize(items, 4);

        QCOMPARE(result.size(), items.size());
        QCOMPARE(result.at(0).slot, 3);
        QCOMPARE(result.at(1).slot, 0);
        QVERIFY(LayoutEngine::isValid(result, 4));
    }

    void validatesPositiveSpansAndUniqueIds()
    {
        QVERIFY(!LayoutEngine::isValid({{"zero", 0, 0}}, 3));
        QVERIFY(!LayoutEngine::isValid({{"duplicate", 0, 1}, {"duplicate", 1, 1}}, 3));
        QVERIFY(LayoutEngine::normalize({{"zero", 0, 0}}, 3).empty());
        QVERIFY(LayoutEngine::normalize({{"duplicate", 0, 1}, {"duplicate", 1, 1}}, 3).empty());
    }
};

QTEST_GUILESS_MAIN(LayoutEngineTest)
#include "layout_engine_test.moc"

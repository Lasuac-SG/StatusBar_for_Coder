#include "core/layout_engine.h"

#include <QtTest>

#include <limits>
#include <vector>

using Core::LayoutEngine;
using Core::LayoutItem;

Q_DECLARE_METATYPE(std::vector<LayoutItem>)

class LayoutEngineTest final : public QObject {
    Q_OBJECT

private slots:
    void swapsDifferentSpansWithoutOverlap()
    {
        const std::vector<LayoutItem> items{{"clock", 0, 3}, {"cpu", 5, 2}};
        const auto result = LayoutEngine::drop(items, "clock", 5, 9);

        QVERIFY(result.has_value());
        QCOMPARE(result->size(), items.size());
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
        QCOMPARE(result->size(), items.size());
        QCOMPARE(result->at(0).slot, 4);
    }

    void clampsNegativeDropTargetToZero()
    {
        const std::vector<LayoutItem> items{{"clock", 2, 2}};
        const auto result = LayoutEngine::drop(items, "clock", -10, 6);

        QVERIFY(result.has_value());
        QCOMPARE(result->size(), items.size());
        QCOMPARE(result->at(0).slot, 0);
    }

    void rejectsDropWithInvalidInput()
    {
        const std::vector<LayoutItem> items{{"clock", -1, 2}};

        QVERIFY(!LayoutEngine::drop(items, "clock", 0, 6).has_value());
    }

    void rejectsUnknownDraggedId()
    {
        const std::vector<LayoutItem> items{{"clock", 0, 2}};

        QVERIFY(!LayoutEngine::drop(items, "missing", 3, 6).has_value());
    }

    void normalizesAfterScreenShrink()
    {
        const std::vector<LayoutItem> items{{"a", 8, 2}, {"b", 1, 3}};
        const auto result = LayoutEngine::normalize(items, 7);

        QVERIFY(LayoutEngine::isValid(result, 7));
        QCOMPARE(result.size(), items.size());
        QCOMPARE(result.at(0).slot, 5);
        QCOMPARE(result.at(1).slot, 1);
    }

    void preservesAlreadyValidLayoutExactly()
    {
        const std::vector<LayoutItem> items{{"a", 5, 1}, {"b", 0, 1}};

        const auto result = LayoutEngine::normalize(items, 6);

        QVERIFY(result == items);
    }

    void returnsEmptyNormalizationWhenCapacityIsInsufficient()
    {
        const std::vector<LayoutItem> items{{"a", 0, 4}, {"b", 4, 4}};

        QVERIFY(LayoutEngine::normalize(items, 7).empty());
    }

    void normalizesUsingNearestFeasibleSlots()
    {
        const std::vector<LayoutItem> items{{"first", 2, 2}, {"second", 2, 1}};
        const auto result = LayoutEngine::normalize(items, 5);

        QCOMPARE(result.size(), items.size());
        QCOMPARE(result.at(0).slot, 2);
        QCOMPARE(result.at(1).slot, 1);
    }

    void normalizesAllItemsInStableInputOrderWhenCapacityFits()
    {
        const std::vector<LayoutItem> items{{"small", 2, 1}, {"wide", 0, 3}};
        const auto result = LayoutEngine::normalize(items, 4);

        QCOMPARE(result.size(), items.size());
        QCOMPARE(result.at(0).slot, 3);
        QCOMPARE(result.at(1).slot, 0);
        QVERIFY(LayoutEngine::isValid(result, 4));
    }

    void normalizesIntMaxSlotCountWithoutScanningSlots()
    {
        constexpr int totalSlots = std::numeric_limits<int>::max();
        const std::vector<LayoutItem> items{
            {"small", totalSlots / 2, 1},
            {"wide", 0, totalSlots - 1},
        };
        const auto result = LayoutEngine::normalize(items, totalSlots);

        QCOMPARE(result.size(), items.size());
        QCOMPARE(result.at(0).slot, 0);
        QCOMPARE(result.at(1).slot, 1);
        QVERIFY(LayoutEngine::isValid(result, totalSlots));
    }

    void rejectsInvalidLayouts_data()
    {
        QTest::addColumn<std::vector<LayoutItem>>("items");
        QTest::addColumn<int>("totalSlots");

        QTest::newRow("negative span") << std::vector<LayoutItem>{{"a", 0, -1}} << 3;
        QTest::newRow("negative slot") << std::vector<LayoutItem>{{"a", -1, 1}} << 3;
        QTest::newRow("overlap")
            << std::vector<LayoutItem>{{"a", 0, 2}, {"b", 1, 2}} << 4;
        QTest::newRow("out of bounds") << std::vector<LayoutItem>{{"a", 2, 2}} << 3;
        QTest::newRow("negative total slots") << std::vector<LayoutItem>{} << -1;
    }

    void rejectsInvalidLayouts()
    {
        QFETCH(std::vector<LayoutItem>, items);
        QFETCH(int, totalSlots);

        QVERIFY(!LayoutEngine::isValid(items, totalSlots));
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

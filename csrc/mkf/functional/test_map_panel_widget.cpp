// RED test: MapPanelWidget API contract test
// Expected result in RED phase: compilation FAILURE (map_panel_widget.h does not exist yet)
// After GREEN phase: this file should compile successfully.

#include "gui/map_panel_widget.h"
#include "core/utils/resource_model.h"
#include <QApplication>
#include <QObject>
#include <QStandardItemModel>
#include <cassert>
#include <fstream>
#include <iostream>

// ---------------------------------------------------------------------------
// Test peer (friend-visible to MapPanelWidget in GREEN phase).
// During RED phase these accesses will FAIL TO COMPILE because the members
// and private method declarations do not yet exist — that's the RED signal.
// ---------------------------------------------------------------------------
struct MapPanelWidgetTester {
    // ---- direct accessors for internal refactor invariants ----
    static int parseCount(const MapPanelWidget& w)       { return w.m_parseCount; }

    // 5 name vectors: sizes must be parallel to corresponding info arrays
    static size_t mapNodeNamesSize   (const MapPanelWidget& w) { return w.m_mapNodeNames.size();    }
    static size_t facilityNamesSize  (const MapPanelWidget& w) { return w.m_facilityNames.size();   }
    static size_t commercialNamesSize(const MapPanelWidget& w) { return w.m_commercialNames.size(); }
    static size_t beautyNamesSize    (const MapPanelWidget& w) { return w.m_beautyNames.size();     }
    static size_t mapNodeTypesSize   (const MapPanelWidget& w) { return w.m_mapNodeTypes.size();    }

    // corresponding info / node arrays
    static size_t mapNodesSize       (const MapPanelWidget& w) { return w.m_mapNodes.size();         }
    static size_t facilityInfosSize  (const MapPanelWidget& w) { return w.m_facilityInfos.size();    }
    static size_t commercialInfosSize(const MapPanelWidget& w) { return w.m_commercialInfos.size();  }
    static size_t beautyInfosSize    (const MapPanelWidget& w) { return w.m_beautyInfos.size();      }

    // loaded identity (for hit test)
    static bool loadedIdentityMatches(const MapPanelWidget& w,
                                      const QModelIndex& i,
                                      const ResourceModel* m) {
        return w.m_loadedIndex == i && w.m_loadedModel == m;
    }

    // phase functions
    static bool ensureLoaded(MapPanelWidget& w, const QModelIndex& i, ResourceModel* m) {
        return w.ensureLoaded(i, m);
    }
    static void redrawScene(MapPanelWidget& w) { w.redrawScene(); }

    // ---------- behaviour tests ----------
    static void runAll() {
        std::cout << "\n--- Refactor tests (parse-once + redraw-many + 5 names) ---"
                  << std::endl;
        test_sameValueNoOpGuard();
        test_ensureLoadedIdentityOnce();
        test_rotationDoesNotReParse();
        test_fiveNameVectorsParallel();
        test_redrawSceneIdempotent();
        std::cout << "ALL REFACTOR TESTS PASSED.\n" << std::endl;
    }

private:
    // ---- Test A: same-value setNorthDirection guard ----
    // setNorthDirection(n) while northDirection()==n must be a no-op:
    //   * no scene work, no ensureLoaded call ⇒ parseCount unchanged
    static void test_sameValueNoOpGuard() {
        auto* panel = new MapPanelWidget();
        const int startDir = panel->northDirection();
        const int countBefore = parseCount(*panel);
        panel->setNorthDirection(startDir);
        assert(panel->northDirection() == startDir);
        assert(parseCount(*panel) == countBefore &&
               "same-value setNorthDirection must not touch parseCount");
        // try all 8 directions as "already set → no-op"
        for (int d = 0; d < 8; d++) {
            panel->setNorthDirection(d);
            const int c = parseCount(*panel);
            panel->setNorthDirection(d);
            assert(parseCount(*panel) == c);
        }
        delete panel;
        std::cout << "[PASS] A. setNorthDirection same-value no-op guard" << std::endl;
    }

    // ---- helper: build a small valid item model so index(row,0).isValid()==true ----
    static void seedDummyModel(QStandardItemModel& model, int rows = 2) {
        for (int r = 0; r < rows; r++) {
            auto* item = new QStandardItem(QStringLiteral("row%1").arg(r));
            model.appendRow(item);
        }
    }

    // ---- Test B: ensureLoaded identity hit skips parse ----
    // Call ensureLoaded twice with the same (index,model):
    //   * first call → new identity, parseCount increments
    //   * second call → identity hit, parseCount stays the same
    static void test_ensureLoadedIdentityOnce() {
        auto* panel = new MapPanelWidget();
        ResourceModel rm;            // empty resource model is fine — parse
                                     // runs even if resources return empty bytes.
        QStandardItemModel dummy;
        seedDummyModel(dummy, 2);    // rows needed: 0 (first identity) + 1 (another identity)
        QModelIndex idx0 = dummy.index(0, 0);   // valid row=0

        const int c0 = parseCount(*panel);
        bool r1 = ensureLoaded(*panel, idx0, &rm);
        std::cout << "  [DEBUG B] r1=" << r1 << " c0=" << c0 << " c1(after first ensureLoaded)=" << parseCount(*panel)
                  << " identity match? " << loadedIdentityMatches(*panel, idx0, &rm) << std::endl;
        Q_UNUSED(r1);
        const int c1 = parseCount(*panel);
        assert(c1 == c0 + 1 && "first ensureLoaded with valid index bumps parseCount");

        bool r2 = ensureLoaded(*panel, idx0, &rm);  // same identity
        assert(r2 == true);
        const int c2 = parseCount(*panel);
        assert(c2 == c1 && "ensureLoaded hit on same identity must NOT re-parse");
        assert(loadedIdentityMatches(*panel, idx0, &rm));

        // Change identity → new parse happens
        QModelIndex idx1 = dummy.index(1, 0);   // row=1, different index
        ensureLoaded(*panel, idx1, &rm);
        const int c3 = parseCount(*panel);
        assert(c3 == c2 + 1 && "different (row) identity triggers another parse");
        assert(loadedIdentityMatches(*panel, idx1, &rm));

        delete panel;
        std::cout << "[PASS] B. ensureLoaded identity hits skip parse; miss re-parses"
                  << std::endl;
    }

    // ---- Test C: rotation does not increase parseCount ----
    // Load a (possibly empty) map, then call setNorthDirection with a
    // different direction. parseCount must NOT move (only redrawScene runs).
    static void test_rotationDoesNotReParse() {
        auto* panel = new MapPanelWidget();
        ResourceModel rm;
        QStandardItemModel dummy;
        seedDummyModel(dummy, 2);
        QModelIndex idx0 = dummy.index(0, 0);
        ensureLoaded(*panel, idx0, &rm);
        const int afterLoad = parseCount(*panel);

        // cycle through 8 directions — none should cause another parse
        for (int d = 0; d < 8; d++) {
            panel->setNorthDirection(d);
            assert(panel->northDirection() == d);
            assert(parseCount(*panel) == afterLoad &&
                   "setNorthDirection with DIFFERENT value must NOT re-parse");
        }
        delete panel;
        std::cout << "[PASS] C. rotation (setNorthDirection) never re-parses" << std::endl;
    }

    // ---- Test D: 5 name-vectors are parallel to info/node arrays ----
    // At any state (empty / after load):
    //   m_mapNodeNames   .size() == m_mapNodes        .size()
    //   m_facilityNames  .size() == m_facilityInfos   .size()
    //   m_commercialNames.size() == m_commercialInfos .size()
    //   m_beautyNames    .size() == m_beautyInfos     .size()
    //   m_mapNodeTypes   .size() == m_mapNodes        .size()
    static void assertParallelSizes(const MapPanelWidget& w, const char* tag) {
        assert(mapNodeNamesSize(w)    == mapNodesSize(w));
        assert(facilityNamesSize(w)   == facilityInfosSize(w));
        assert(commercialNamesSize(w) == commercialInfosSize(w));
        assert(beautyNamesSize(w)     == beautyInfosSize(w));
        assert(mapNodeTypesSize(w)    == mapNodesSize(w));
        Q_UNUSED(tag);
    }
    static void test_fiveNameVectorsParallel() {
        auto* panel = new MapPanelWidget();
        // 1. fresh state — all zeros, parallel
        assertParallelSizes(*panel, "initial");

        // 2. loaded (possibly empty) state — still parallel
        ResourceModel rm;
        QStandardItemModel dummy;
        seedDummyModel(dummy, 2);
        ensureLoaded(*panel, dummy.index(0, 0), &rm);
        assertParallelSizes(*panel, "after-ensureLoaded");

        // 3. after a couple of rotations — still parallel
        panel->setNorthDirection(3);
        assertParallelSizes(*panel, "after-rotation-1");
        panel->setNorthDirection(6);
        assertParallelSizes(*panel, "after-rotation-2");

        delete panel;
        std::cout << "[PASS] D. 5 name vectors stay parallel to info/node arrays"
                  << std::endl;
    }

    // ---- Test E: redrawScene can be called standalone any number of times ----
    // Calling redrawScene() multiple times must be safe (clear + rebuild),
    // and must never re-parse (parseCount stays put).
    static void test_redrawSceneIdempotent() {
        auto* panel = new MapPanelWidget();
        ResourceModel rm;
        QStandardItemModel dummy;
        seedDummyModel(dummy, 2);
        ensureLoaded(*panel, dummy.index(0, 0), &rm);
        const int c0 = parseCount(*panel);

        redrawScene(*panel);
        redrawScene(*panel);
        redrawScene(*panel);
        assert(parseCount(*panel) == c0 &&
               "repeated redrawScene calls must never increment parseCount");
        delete panel;
        std::cout << "[PASS] E. redrawScene is idempotent and never re-parses"
                  << std::endl;
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // 1. 基本构造
    MapPanelWidget* panel = new MapPanelWidget();
    assert(panel != nullptr);
    std::cout << "[PASS] Constructor (no parent)" << std::endl;

    MapPanelWidget* panelWithParent = new MapPanelWidget(panel);
    assert(panelWithParent != nullptr);
    std::cout << "[PASS] Constructor (with parent)" << std::endl;

    // 2. 核心公共槽：函数签名必须可调用（调用时传无效 index 不会 crash，最多 emit 错误）
    //    这里只验证"能被调用、签名匹配"，不依赖真实 ResourceModel
    panel->clear();
    std::cout << "[PASS] clear() callable" << std::endl;

    // 3. 信号必须能被 connect 到（验证信号签名正确）
    bool gotText = false;
    bool gotStatus = false;
    bool gotNodeSelected = false;
    QObject::connect(panel, &MapPanelWidget::mapTextReady,
                     [&](const QString& t) { gotText = true; Q_UNUSED(t); });
    QObject::connect(panel, &MapPanelWidget::statusMessage,
                     [&](const QString& t) { gotStatus = true; Q_UNUSED(t); });
    QObject::connect(panel, &MapPanelWidget::nodeSelected,
                     [&](int idx, const MapNode& n) { gotNodeSelected = true; Q_UNUSED(idx); Q_UNUSED(n); });
    std::cout << "[PASS] Signals connectable: mapTextReady, statusMessage, nodeSelected" << std::endl;

    // 4. 预留扩展接口：North 方向
    panel->setNorthDirection(0);
    assert(panel->northDirection() == 0);
    panel->setNorthDirection(7);
    assert(panel->northDirection() == 7);
    std::cout << "[PASS] setNorthDirection / northDirection (0..7 range placeholder)" << std::endl;

    // 5. 预留扩展接口：分层显示
    panel->setLayerVisible(2000, 3999, true);
    panel->setLayerVisible(4000, 5999, false);
    std::cout << "[PASS] setLayerVisible callable" << std::endl;

    delete panelWithParent;
    delete panel;

    std::cout << "ALL API CONTRACT TESTS PASSED." << std::endl;

    // ---- NEW: refactor contract tests (RED → GREEN) ----
    MapPanelWidgetTester::runAll();

    return 0;
}

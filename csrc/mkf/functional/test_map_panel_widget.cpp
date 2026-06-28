// RED test: MapPanelWidget API contract test
// Expected result in RED phase: compilation FAILURE (map_panel_widget.h does not exist yet)
// After GREEN phase: this file should compile successfully.

#include "gui/map_panel_widget.h"
#include "core/utils/resource_model.h"
#include <QApplication>
#include <QObject>
#include <cassert>
#include <iostream>

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
    return 0;
}

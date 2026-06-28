#pragma once

#include "gui/map_graphics_view.h"
#include "core/types/map.h"
#include "core/utils/resource_model.h"
#include <QGraphicsScene>
#include <QModelIndex>
#include <QWidget>
#include <vector>

class MapPanelWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapPanelWidget(QWidget* parent = nullptr);
    ~MapPanelWidget() override;

    // --- 预留扩展接口（Ground 旋转/North 方向，先占位，后续实现） ---
    // topLeftIndex: 0..7，对应 project_memory 中定义的 Sprite TopLeftIndex
    void setNorthDirection(int topLeftIndex);
    int  northDirection() const;

    // --- 预留扩展接口（分层显示，先占位，后续实现） ---
    // nodeType 区间对应 MapNode::type:
    //   2000~3999: 普通土地
    //   4000~5999: 设施
    //   6001~7999: 上市企业
    void setLayerVisible(int nodeTypeMin, int nodeTypeMax, bool visible);

public slots:
    // 加载一个 MAP 资源并渲染到场景，同时通过 mapTextReady 输出节点列表文本
    void loadMap(const QModelIndex& mapIndex, ResourceModel* resourceModel);

    // 清空场景和节点缓存
    void clear();

signals:
    // 右栏文本：节点列表、缩放提示等
    void mapTextReady(const QString& text);

    // 状态栏：鼠标 XY 坐标、错误信息等
    void statusMessage(const QString& message);

    // 预留：节点被点击/选中时发出（后续实现交互再接线）
    void nodeSelected(int nodeIndex, const MapNode& node);

private slots:
    // MapGraphicsView 鼠标移动回调：转发场景坐标
    void onMousePositionChanged(int x, int y);

private:
    void setupUI();

    // 把 displayMap 拆成两块：
    //   populateScene: 负责往 QGraphicsScene 里塞节点图片/文字/圆点（原来的 displayMap 主体）
    //   buildMapText:  负责生成节点列表文本（原来的 displayMapText，返回 QString 不直接写 UI）
    void populateScene(const QModelIndex& index, ResourceModel* resourceModel);
    QString buildMapText(const QModelIndex& index, ResourceModel* resourceModel);

private:
    // --- 状态 ---
    std::vector<MapNode> m_mapNodes;
    int m_northDirection;   // 预留：TopLeftIndex 0..7，占位=0

    // --- UI ---
    MapGraphicsView* m_mapView;
    QGraphicsScene* m_mapScene;
};

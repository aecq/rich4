#pragma once

#include "gui/map_graphics_view.h"
#include "core/types/map.h"
#include "core/utils/resource_model.h"
#include <QGraphicsScene>
#include <QModelIndex>
#include <QWidget>
#include <utility>
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

    // --- 旋转枢轴（运行时可改；非编译期常量） ---
    // "容器中心的 Scene 点位置不变"即指绕 (pivotX, pivotY) 旋转后它在画布坐标系下坐标不动。
    // 初值 = kSceneSize / 2；populateScene 可根据地图内容包围盒或视口中心重新 setPivot。
    void setPivot(float pivotX, float pivotY);
    float pivotX() const;
    float pivotY() const;

    // --- 与旋转/Chunk 映射相关的公共常量（外部读出来做坐标换算时用） ---
    // 场景默认尺寸（初始 sceneRect 边长；populateScene 里目前仍按此设定）
    static constexpr int kSceneSize = 2300;
    // TopLeftIndex 每增减 1 对应的旋转角度（X+ 为初始极向量，逆时针为正）
    static constexpr int kAngleStepDeg = 45;
    // TopLeftIndex 的合法范围
    static constexpr int kTopLeftIndexMin = 0;
    static constexpr int kTopLeftIndexMax = 7;
    // 初始朝向：North = Top Left（即"地图的北" 指向 "画布的左上角" 时的 TopLeftIndex）
    // 实际值需按资源校准；先按 0 占位，后面不对就只改这一个数字。
    static constexpr int kNorthTopLeftIndex = 0;

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
    void onNorthRotateBy(int delta);

private:
    void setupUI();

    // 把 displayMap 拆成两块：
    //   populateScene: 负责往 QGraphicsScene 里塞节点图片/文字/圆点（原来的 displayMap 主体）
    //   buildMapText:  负责生成节点列表文本（原来的 displayMapText，返回 QString 不直接写 UI）
    void populateScene(const QModelIndex& index, ResourceModel* resourceModel);
    QString buildMapText(const QModelIndex& index, ResourceModel* resourceModel);

    // --- 通用坐标旋转辅助函数 ---
    // 把"地理坐标 (gx, gy)"按当前 TopLeftIndex 绕枢轴 (m_pivotX, m_pivotY) 旋转到"画布坐标"。
    // 约定：X+ 为初始极向量方向，逆时针为正；TopLeftIndex 每 +1 角度 +kAngleStepDeg。
    // 传负的 topLeftIndex 可实现反变换（画布坐标 → 地理坐标）。
    std::pair<float, float> rotateAround(float gx, float gy, int topLeftIndex) const;

    // --- 通用 Chunk 索引偏移辅助函数 ---
    // 约定（用户定义）：一组资源共 S 个 chunk（当前使用 S = 1 / 2 / 8；公式本身通用）。
    //   base     = chunk[0] 在该资源数组中的绝对下标（= "减 chunk[0] 在 resource 中的下标" 的基准）
    //   absChunk = 当前 chunk 的绝对下标（满足 k = absChunk - base ∈ [0, S-1]）
    //   i        = 整数偏移（例如 = topLeftIndex）
    //   新相对下标 k' = (k + i) % S   （取非负的正余数 [0, S-1]）
    //   返回新绝对下标 = base + k'
    // S=1 时 k 恒为 0，结果恒等于 base（任何 i 都不改变 chunk）。
    int offsetChunkN(int absChunk, int base, int S, int i) const;

    // 便捷包装：S=8（Sprite 8 方图 / MapNode.chunk）。
    int offsetChunk8(int absChunk, int base, int i) const;

    // 便捷包装：S=2（large tile 两个 chunk）。
    int offsetChunk2(int absChunk, int base, int i) const;

private:
    // --- 状态 ---
    std::vector<MapNode> m_mapNodes;
    int m_northDirection;   // TopLeftIndex 0..7；初值 = kNorthTopLeftIndex
    float m_pivotX;         // 旋转枢轴 X（画布/场景坐标，运行时可改）
    float m_pivotY;         // 旋转枢轴 Y
    QModelIndex m_mapIndex;
    ResourceModel* m_resourceModel;

    // --- UI ---
    MapGraphicsView* m_mapView;
    QGraphicsScene* m_mapScene;
};

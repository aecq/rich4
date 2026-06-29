#include "gui/map_panel_widget.h"
#include "core/io/parse.h"
#include "core/types/graph_info.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QBitmap>
#include <QPixmap>
#include <QVBoxLayout>

MapPanelWidget::MapPanelWidget(QWidget* parent)
    : QWidget(parent)
    , m_northDirection(0)
    , m_mapView(nullptr)
    , m_mapScene(nullptr)
{
    setupUI();
}

MapPanelWidget::~MapPanelWidget() {
}

// ---------------------------------------------------------------------------
// UI 初始化
// ---------------------------------------------------------------------------
void MapPanelWidget::setupUI() {
    // 1. 铺满整个 MapPanelWidget 的布局
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 2. 场景 + 视图
    m_mapView = new MapGraphicsView(this);
    m_mapScene = new QGraphicsScene(m_mapView);
    m_mapView->setScene(m_mapScene);

    m_mapView->setDragMode(QGraphicsView::ScrollHandDrag);      // 拖拽平移
    m_mapView->setInteractive(true);
    m_mapView->setRenderHint(QPainter::Antialiasing);
    m_mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_mapView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);

    layout->addWidget(m_mapView);

    // 3. 鼠标位置信号转发
    connect(m_mapView, &MapGraphicsView::mousePositionChanged,
            this, &MapPanelWidget::onMousePositionChanged);
}

// ---------------------------------------------------------------------------
// 公共槽：加载 MAP 资源
// ---------------------------------------------------------------------------
void MapPanelWidget::loadMap(const QModelIndex& mapIndex, ResourceModel* resourceModel) {
    clear();
    if (!mapIndex.isValid() || resourceModel == nullptr) {
        emit mapTextReady(QString());
        return;
    }

    populateScene(mapIndex, resourceModel);

    QString text = buildMapText(mapIndex, resourceModel);
    emit mapTextReady(text);
}

// ---------------------------------------------------------------------------
// 公共槽：清空
// ---------------------------------------------------------------------------
void MapPanelWidget::clear() {
    m_mapNodes.clear();
    if (m_mapScene) {
        m_mapScene->clear();
    }
    emit mapTextReady(QString());
}

// ---------------------------------------------------------------------------
// 预留接口：North 方向（占位，后续 Ground 旋转实现时再充实）
// ---------------------------------------------------------------------------
void MapPanelWidget::setNorthDirection(int topLeftIndex) {
    // clamp 到 0..7
    if (topLeftIndex < 0) topLeftIndex = 0;
    if (topLeftIndex > 7) topLeftIndex = 7;
    m_northDirection = topLeftIndex;
    // TODO: Ground 旋转重新计算 chunk 索引 + 场景旋转矩阵
}

int MapPanelWidget::northDirection() const {
    return m_northDirection;
}

// ---------------------------------------------------------------------------
// 预留接口：分层显示（占位，后续图层管理实现时再充实）
// ---------------------------------------------------------------------------
void MapPanelWidget::setLayerVisible(int nodeTypeMin, int nodeTypeMax, bool visible) {
    Q_UNUSED(nodeTypeMin);
    Q_UNUSED(nodeTypeMax);
    Q_UNUSED(visible);
    // TODO: 按 MapNode::type 区间控制对应 QGraphicsItem 的 setVisible
}

// ---------------------------------------------------------------------------
// 鼠标位置：转发到状态栏
// ---------------------------------------------------------------------------
void MapPanelWidget::onMousePositionChanged(int x, int y) {
    QString text = QString("Map Scene XY: %1, %2").arg(x).arg(y);
    emit statusMessage(text);
}

// ===========================================================================
// 私有：场景渲染（逻辑来自 GraphicsTextWindow::displayMap）
// ===========================================================================
void MapPanelWidget::populateScene(const QModelIndex& index, ResourceModel* resourceModel) {
    m_mapView->show();

    // 解析节点
    QByteArray bytes = resourceModel->getResource(index.row());
    m_mapNodes = parseMapNodes(bytes, 0);

    // 统计 GND 资源数量（到第一个 SMP/SPR 为止）
    int count = 0;
    for (int i = 0; i < resourceModel->n(); i++) {
        if (resourceModel->getSignature(i).startsWith("GND")) {
            count++;
        } else if (resourceModel->getSignature(i).startsWith("SMP")
                || resourceModel->getSignature(i).startsWith("SPR")) {
            break;
        }
    }
    std::vector<QImage> nodeImages = parseImages(resourceModel->getResource(3 * count));
    std::vector<GraphInfo> nodeInfos  = parseGraphInfos(resourceModel->getResource(3 * count));
    const QColor TRANSPARENT(Qt::black);

    m_mapScene->clear();

    if (m_mapNodes.empty()) return;

    // 场景大小 + 初始缩放
    const int sceneSize = 2300;
    float factor = 1.0f * m_mapView->width() / sceneSize;
    m_mapView->scale(factor, factor);

    // 按节点绘制
    for (size_t i = 0; i < m_mapNodes.size(); i++) {
        const MapNode& node = m_mapNodes[i];

        float x = node.x;
        float y = node.y;

        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));

        // 图片
        QString type = resourceModel->getType(index.row());
        int chunkOffset = type.mid(3, type.length() - 3).toInt();
        int chunk = node.chunk + chunkOffset;
        const bool hasImage = (node.special > 0 && chunk < (int)nodeImages.size() && !nodeImages[chunk].isNull())
                           || (node.special <= 0 && 0 < chunk && chunk < (int)nodeImages.size());
        if (hasImage) {
            QPixmap pixmap = QPixmap::fromImage(nodeImages[chunk]);
            QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
            pixmap.setMask(mask);
            QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
            pixmapItem->setPos(x - nodeInfos[chunk].x, y - nodeInfos[chunk].y);
        }

        // 颜色映射：type / 2000 取索引
        QColor colors[5] = { Qt::gray, Qt::gray, Qt::cyan, Qt::cyan, Qt::cyan };
        const int denominator = 2000;

        // 地名文本
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + ((node.special > 0) ? nodeInfos[chunk].y : 0));
            textItem->setDefaultTextColor(colors[node.type / denominator]);
        }

        // 类型数字
        if (node.type != 0) {
            QString typeStr = QString::number(node.type);
            QGraphicsTextItem* typeItem = m_mapScene->addText(typeStr);
            typeItem->setPos(x - typeItem->boundingRect().width() / 2,
                             y - ((node.special > 0) ? nodeInfos[chunk].y : 0)
                               - typeItem->boundingRect().height());
            typeItem->setDefaultTextColor(colors[node.type / denominator]);
        }

        // 非特殊节点、无图片：补一个灰点
        if (node.special <= 0 && chunk <= 0) {
            QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
            dot->setBrush(Qt::gray);
        }
    }

    // 绘制设施节点
    std::vector<FacilityNode> facilityNodes = parseFacilityNodes(bytes);
    for (const FacilityNode& node : facilityNodes) {
        float x = node.x;
        float y = node.y;
        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::red);
        }
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::gray);
    }

    // 绘制上市企业节点
    std::vector<CommercialNode> commercialNodes = parseCommercialNodes(bytes);
    for (const CommercialNode& node : commercialNodes) {
        float x = node.x;
        float y = node.y;
        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::blue);
        }
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::gray);
    }

    // 绘制美观节点
    std::vector<BeautyNode> beautyNodes = parseBeautyNodes(bytes);
    for (const BeautyNode& node : beautyNodes) {
        float x = node.x;
        float y = node.y;
        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::green);
        }
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::green);
    }

    m_mapScene->setSceneRect(0, 0, sceneSize, sceneSize);
    m_mapView->fitInView(m_mapScene->sceneRect(), Qt::KeepAspectRatio);
}

// ===========================================================================
// 私有：生成右侧节点列表文本（逻辑来自 GraphicsTextWindow::displayMapText）
// ===========================================================================
QString MapPanelWidget::buildMapText(const QModelIndex& index, ResourceModel* resourceModel) {
    QString text;
    text += resourceModel->getType(index.row());
    text += "\nScale: CTRL + Wheel";
    text += QString("\nMap Node Count: %1").arg(m_mapNodes.size()) + "\n";
    for (int i = 0; i < (int)m_mapNodes.size(); i++) {
        text += QString("\n%1 (%2, %3) %4: %5")
            .arg(i, 3, 10, QChar(' '))
            .arg(m_mapNodes[i].x)
            .arg(m_mapNodes[i].y)
            .arg(m_mapNodes[i].type, 4, 10, QChar(' '))
            .arg(parseBig5Trim(QByteArray::fromRawData(
                m_mapNodes[i].name, sizeof(MapNode::name))));
        text += QString(" %1 %2 %3 %4\n")
            .arg(m_mapNodes[i].neighbors[0], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[1], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[2], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[3], 2, 10, QChar(' '));
    }
    return text;
}

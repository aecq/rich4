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
#include <QtMath>

MapPanelWidget::MapPanelWidget(QWidget* parent)
    : QWidget(parent)
    , m_northDirection(kNorthTopLeftIndex)
    , m_pivotX(static_cast<float>(kSceneSize) * 0.5f)
    , m_pivotY(static_cast<float>(kSceneSize) * 0.5f)
    , m_mapView(nullptr)
    , m_mapScene(nullptr)
    , m_resourceModel(nullptr)
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
    m_mapScene->setSceneRect(0, 0, kSceneSize, kSceneSize);
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

    connect(m_mapView, &MapGraphicsView::northRotateBy,
            this, &MapPanelWidget::onNorthRotateBy);
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

    // 首次加载 MAP：重置视图以适配场景，并保证 pivot 对齐到视口中心
    // m_mapView->fitInView(m_mapScene->sceneRect(), Qt::KeepAspectRatio);
    // 适配真实内容（不加 padding 的版本，初始视图更紧凑）
    QRectF contentRect = m_mapScene->itemsBoundingRect();
    if (contentRect.isNull()) {
        contentRect = QRectF(0, 0, kSceneSize, kSceneSize);
    }
    m_mapView->fitInView(contentRect, Qt::KeepAspectRatio);
    m_mapView->centerOn(m_pivotX, m_pivotY);

    QString text = buildMapText(mapIndex, resourceModel);
    emit mapTextReady(text);

    m_mapIndex = mapIndex;
    m_resourceModel = resourceModel;
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

    m_mapIndex = QModelIndex();
    m_resourceModel = nullptr;
}

// ---------------------------------------------------------------------------
// 预留接口：North 方向（占位，后续 Ground 旋转实现时再充实）
// ---------------------------------------------------------------------------
void MapPanelWidget::setNorthDirection(int topLeftIndex) {
    // clamp 到 kTopLeftIndexMin..kTopLeftIndexMax
    if (topLeftIndex < kTopLeftIndexMin) topLeftIndex = kTopLeftIndexMin;
    if (topLeftIndex > kTopLeftIndexMax) topLeftIndex = kTopLeftIndexMax;
    m_northDirection = topLeftIndex;
    // TODO: Ground 旋转重新计算 chunk 索引 + 场景旋转矩阵
    if (m_mapIndex.isValid() && m_resourceModel != nullptr) {
        populateScene(m_mapIndex, m_resourceModel);
    }
}

int MapPanelWidget::northDirection() const {
    return m_northDirection;
}

// ---------------------------------------------------------------------------
// 旋转枢轴 getter / setter
// ---------------------------------------------------------------------------
void MapPanelWidget::setPivot(float pivotX, float pivotY) {
    m_pivotX = pivotX;
    m_pivotY = pivotY;
}

float MapPanelWidget::pivotX() const {
    return m_pivotX;
}

float MapPanelWidget::pivotY() const {
    return m_pivotY;
}

// ---------------------------------------------------------------------------
// 通用旋转 / Chunk 偏移辅助函数
// ---------------------------------------------------------------------------
std::pair<float, float> MapPanelWidget::rotateAround(float gx, float gy, int topLeftIndex) const {
    const qreal theta = qDegreesToRadians(static_cast<qreal>(topLeftIndex) * kAngleStepDeg - 22.5);
    const qreal dx = static_cast<qreal>(gx) - m_pivotX;
    const qreal dy = static_cast<qreal>(gy) - m_pivotY;
    const qreal cosT = qCos(theta);
    const qreal sinT = qSin(theta);
    const float nx = static_cast<float>(m_pivotX + dx * cosT - dy * sinT);
    const float ny = static_cast<float>(m_pivotY + dx * sinT + dy * cosT);
    return {nx, ny};
}

int MapPanelWidget::offsetChunkN(int absChunk, int base, int S, int i) const {
    // 用户定义：k' = (k - i) % S，返回 base + k'。
    //   S        = 该资源的 chunk 总数（当前使用 1 / 2 / 8；公式本身不限制 S）
    //   k        = absChunk - base  ∈ [0, S-1]
    //   结果 k'  = 非负的正余数 [0, S-1]
    Q_ASSERT(S > 0);
    const int k = absChunk - base;
    int kNew = (k - i) % S;
    // C++11 起 % 对负数是向零取整（-3 % 8 = -3），这里转成正余数。
    if (kNew < 0) {
        kNew += S;
    }
    // 约定校验：验证后如果方向反了，把 (k - i) 改成 (k + i) 即可（两处一起改）。
    return base + kNew;
}

int MapPanelWidget::offsetChunk8(int absChunk, int base, int i) const {
    return offsetChunkN(absChunk, base, 8, i);
}

int MapPanelWidget::offsetChunk2(int absChunk, int base, int i) const {
    return offsetChunkN(absChunk, base, 2, i);
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

void MapPanelWidget::onNorthRotateBy(int delta) {
    // 方向是环形的 0..7，用正余数做循环：0-1=7，7+1=0
    // 若想要"到边界就停止"的 clamp 语义，直接改为：
    //   setNorthDirection(m_northDirection + delta);
    const int S = kTopLeftIndexMax - kTopLeftIndexMin + 1;  // = 8
    int next = m_northDirection + delta;
    next = ((next % S) + S) % S;
    setNorthDirection(next);
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
    const int MAP_SPRITE_OFFSET = 14;
    // 地块
    const int TILE_OFFSET = 2;
    const int LARGE_TILE_CHUNK_OFFSET = 2;
    std::vector<GraphInfo> tileInfos = parseGraphInfos(resourceModel->getResource(3 * count + TILE_OFFSET));
    std::vector<QImage> tileImages = parseImages(resourceModel->getResource(3 * count + TILE_OFFSET));

    m_mapScene->clear();

    if (m_mapNodes.empty()) return;

    // 按节点绘制
    for (size_t i = 0; i < m_mapNodes.size(); i++) {
        const MapNode& node = m_mapNodes[i];

        // 地理坐标 (node.x, node.y) → 画布坐标（绕 pivot 按当前 North 旋转）
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(node.x),
                                                        static_cast<float>(node.y),
                                                        m_northDirection);
        float x = canvasXY.first;
        float y = canvasXY.second;

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
    std::vector<FacilityInfo> facilityInfos = parseFacilityInfos(bytes);
    for (const FacilityInfo& item : facilityInfos) {
        // 地理坐标 → 画布坐标
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        float x = canvasXY.first;
        float y = canvasXY.second;
        // 地块（large tile，S=2；chunk[0] 在 tileImages 的下标 = LARGE_TILE_CHUNK_OFFSET）
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        QPixmap tilePixmap = QPixmap::fromImage(tileImages[tileChunk]);
        QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
        tilePixmap.setMask(tileMask);
        QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
        tilePixmapItem->setPos(x - tileInfos[tileChunk].x, y - tileInfos[tileChunk].y);
        // 名称
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::red);
        }
        // 可视化定位点
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::gray);
    }

    // 绘制上市企业节点
    std::vector<CommercialInfo> commercialInfos = parseCommercialInfos(bytes);
    for (const CommercialInfo& item : commercialInfos) {
        // 地理坐标 → 画布坐标
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        float x = canvasXY.first;
        float y = canvasXY.second;
        // 地块（large tile，S=2）
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        QPixmap tilePixmap = QPixmap::fromImage(tileImages[tileChunk]);
        QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
        tilePixmap.setMask(tileMask);
        QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
        tilePixmapItem->setPos(x - tileInfos[tileChunk].x, y - tileInfos[tileChunk].y);
        // 图片（Sprite 8 方图；chunk[0] 在该 sprite 资源 images[] 的下标 = 0，即 base=0）
        const int16_t spriteOffset = item.sprite;
        if (spriteOffset <= 0) continue;
        const int chunk = offsetChunk8(item.face, 0, m_northDirection);
        const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
        if (spriteResourceIndex <= 0 || spriteResourceIndex >= resourceModel->n()) continue;
        const QString type = resourceModel->getType(spriteResourceIndex);
        if (!type.startsWith("SPR") && !type.startsWith("SMP")) continue;
        std::vector<GraphInfo> infos = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
        if (chunk < 0 || chunk >= (int)infos.size()) continue;
        std::vector<QImage> images = parseImages(resourceModel->getResource(spriteResourceIndex));
        if (images[chunk].isNull()) continue;
        QPixmap pixmap = QPixmap::fromImage(images[chunk]);
        QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
        pixmap.setMask(mask);
        QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
        pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
        // 名称
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::blue);
        }
        // 可视化定位点
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::gray);
    }

    // 绘制美观节点
    std::vector<BeautyInfo> beautyInfos = parseBeautyInfos(bytes);
    for (const BeautyInfo& item : beautyInfos) {
        // 地理坐标 → 画布坐标
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        float x = canvasXY.first;
        float y = canvasXY.second;
        // 图片（Sprite 8 方图；base=0）
        const int16_t spriteOffset = item.sprite;
        if (spriteOffset <= 0) continue;
        const int chunk = offsetChunk8(item.face, 0, m_northDirection);
        const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
        if (spriteResourceIndex <= 0 || spriteResourceIndex >= resourceModel->n()) continue;
        const QString type = resourceModel->getType(spriteResourceIndex);
        if (!type.startsWith("SPR") && !type.startsWith("SMP")) continue;
        std::vector<GraphInfo> infos = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
        if (chunk < 0 || chunk >= (int)infos.size()) continue;
        std::vector<QImage> images = parseImages(resourceModel->getResource(spriteResourceIndex));
        if (images[chunk].isNull()) continue;
        QPixmap pixmap = QPixmap::fromImage(images[chunk]);
        QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
        pixmap.setMask(mask);
        QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
        pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
        // 名称
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));
        if (!name.isEmpty()) {
            QGraphicsTextItem* textItem = m_mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2,
                             y + textItem->boundingRect().height() / 2);
            textItem->setDefaultTextColor(Qt::green);
        }
        // 可视化定位点
        QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
        dot->setBrush(Qt::green);
    }

    // 根据实际绘制内容调整 sceneRect，确保放大后可滚动到所有边缘
    QRectF contentRect = m_mapScene->itemsBoundingRect();
    if (contentRect.isNull()) {
        // 场景为空（m_mapNodes 空）时兜底，保证 sceneRect 不为空
        contentRect = QRectF(0, 0, kSceneSize, kSceneSize);
    }
    // 四周加 padding（= 放大后的余量；缩得越大，padding 相对越小但绝对滚动范围存在）
    const qreal padding = 200.0;
    contentRect.adjust(-padding, -padding, padding, padding);
    // 确保旋转枢轴在 sceneRect 内（否则后续 centerOn 会被 clamp 到边界导致视角不对）
    contentRect = contentRect.united(QRectF(m_pivotX, m_pivotY, 1.0, 1.0));
    m_mapScene->setSceneRect(contentRect);
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

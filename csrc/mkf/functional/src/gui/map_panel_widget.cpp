#include "gui/map_panel_widget.h"
#include "core/io/parse.h"
#include "core/types/graph_info.h"
#include <QGraphicsEllipseItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsTextItem>
#include <QPainter>
#include <QBitmap>
#include <QPixmap>
#include <QCursor>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>
#include <functional>

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
    std::pair<float, float> mapXY = rotateAround(static_cast<float>(x),
                                                 static_cast<float>(y),
                                                 1 - m_northDirection);
    QString text = QString("Canvas XY (%1, %2) Map XY (%3, %4)").arg(x).arg(y)
                                .arg(int(mapXY.first)).arg(int(mapXY.second));
    emit statusMessage(text);
}

void MapPanelWidget::onNorthRotateBy(int delta) {
    // ================================================================
    // 阶段 1：旋转前，记录"视觉保持不动"的点。
    //   - 鼠标当前在 GraphicsView 视口内 → 取鼠标像素点
    //   - 否则                         → 取视口中心（兼容旧行为）
    // 把这个像素映射到 Scene，再反变换到地理坐标 G_geo：
    //   G_geo 才是"真实地图上那个点"，旋转前后应当仍落在同一个视口像素上。
    // ================================================================
    QWidget* vp = m_mapView->viewport();
    const QRect vpRect = vp->rect();
    const QPoint vpCenter = vpRect.center();

    QPoint Vp;
    const QPoint localCursor = vp->mapFromGlobal(QCursor::pos());
    if (vpRect.contains(localCursor)) {
        Vp = localCursor;
    } else {
        Vp = vpCenter;
    }

    const QPointF spPrev = m_mapView->mapToScene(Vp);
    const int T_old = m_northDirection;
    // 反变换：地理坐标 = rotateAround(画布坐标, 1 - T)
    const std::pair<float, float> G_geo =
        rotateAround(static_cast<float>(spPrev.x()),
                     static_cast<float>(spPrev.y()),
                     1 - T_old);

    // ================================================================
    // 阶段 2：执行 TopLeftIndex 切换（环形 0..7）。
    // setNorthDirection 内部会触发 populateScene，
    // 所有节点按新朝向重新 setPos + 换 chunk，（如有）sceneRect 也会更新。
    // ================================================================
    const int S = kTopLeftIndexMax - kTopLeftIndexMin + 1;  // = 8
    int next = T_old + delta;
    next = ((next % S) + S) % S;
    setNorthDirection(next);

    // ================================================================
    // 阶段 3：旋转后，把 G_geo 在新朝向下的画布坐标"搬回"视口像素 Vp。
    //   键盘旋转不改变 scale，所以只需按当前缩放比把"像素偏移"换算成 Scene 偏移，
    //   让 view 的 center 对准 S_next 减去该偏移即可。
    // ================================================================
    const int T_new = m_northDirection;
    const std::pair<float, float> S_next =
        rotateAround(G_geo.first, G_geo.second, T_new);
    const QPointF spNext(static_cast<qreal>(S_next.first),
                         static_cast<qreal>(S_next.second));

    // 缩放比：Scene 1 单位 = View 多少像素。奇异时（极罕见）兜底 1.0。
    const QTransform M = m_mapView->transform();
    const qreal sx = (qFuzzyIsNull(M.m11())) ? 1.0 : 1.0 / M.m11();
    const qreal sy = (qFuzzyIsNull(M.m22())) ? 1.0 : 1.0 / M.m22();

    const QPoint offsetPx = Vp - vpCenter;
    const QPointF targetCenter(spNext.x() - offsetPx.x() * sx,
                               spNext.y() - offsetPx.y() * sy);

    m_mapView->centerOn(targetCenter);
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

    // ------------------------------------------------------------------
    // 分层深度排序（画家算法 + 层级）：
    //   Layer 10 = 贴地层   → MapNode（地面贴图，永远在立体物体之下）
    //   Layer 20 = 立体物体层 → Facility/Commercial/Beauty（建筑 / 地块 / 装饰精灵）
    // 排序规则：先按 layer 升序（低层先画被高层盖住），同 layer 再按画布 y 升序
    // （y 小先画被 y 大的盖 → 同层内近景压远景）。
    // ------------------------------------------------------------------
    struct DrawEntry {
        int layer;
        float sortY;
        std::function<void()> paint;
    };
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(m_mapNodes.size()
                        + parseFacilityInfos(bytes).size()
                        + parseCommercialInfos(bytes).size()
                        + parseBeautyInfos(bytes).size());

    // MapNode 中不变的参数提到循环外
    QString nodeType = resourceModel->getType(index.row());
    int chunkOffset = nodeType.mid(3, nodeType.length() - 3).toInt();
    QColor colors[5] = { Qt::gray, Qt::gray, Qt::cyan, Qt::cyan, Qt::cyan };
    const int denominator = 2000;

    // 按节点绘制 → 收集（Layer 10: 贴地）
    for (size_t i = 0; i < m_mapNodes.size(); i++) {
        const MapNode& node = m_mapNodes[i];
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(node.x),
                                                        static_cast<float>(node.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;

        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));
        int chunk = node.chunk + chunkOffset;
        const bool hasImage = (node.special > 0 && chunk < (int)nodeImages.size() && !nodeImages[chunk].isNull())
                           || (node.special <= 0 && 0 < chunk && chunk < (int)nodeImages.size());

        drawEntries.push_back(DrawEntry{
            10,  // Layer 10 = 贴地层
            y,
            [this, x, y, name, chunk, hasImage, node, chunkOffset, TRANSPARENT,
             colors, denominator,
             &nodeImages, &nodeInfos]() {
                if (hasImage) {
                    QPixmap pixmap = QPixmap::fromImage(nodeImages[chunk]);
                    QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                    pixmap.setMask(mask);
                    QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                    pixmapItem->setPos(x - nodeInfos[chunk].x, y - nodeInfos[chunk].y);
                }
                if (!name.isEmpty()) {
                    QGraphicsTextItem* textItem = m_mapScene->addText(name);
                    textItem->setPos(x - textItem->boundingRect().width() / 2,
                                     y + ((node.special > 0) ? nodeInfos[chunk].y : 0));
                    textItem->setDefaultTextColor(colors[node.type / denominator]);
                }
                if (node.type != 0) {
                    QString typeStr = QString::number(node.type);
                    QGraphicsTextItem* typeItem = m_mapScene->addText(typeStr);
                    typeItem->setPos(x - typeItem->boundingRect().width() / 2,
                                     y - ((node.special > 0) ? nodeInfos[chunk].y : 0)
                                       - typeItem->boundingRect().height());
                    typeItem->setDefaultTextColor(colors[node.type / denominator]);
                }
                if (node.special <= 0 && chunk <= 0) {
                    QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
                    dot->setBrush(Qt::gray);
                }
            }
        });
    }

    // 绘制设施节点 → 收集（Layer 20: 立体/地面地块）
    std::vector<FacilityInfo> facilityInfos = parseFacilityInfos(bytes);
    for (const FacilityInfo& item : facilityInfos) {
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));

        drawEntries.push_back(DrawEntry{
            20,  // Layer 20 = 立体物体层
            y,
            [this, x, y, tileChunk, name, TRANSPARENT,
             &tileImages, &tileInfos]() {
                QPixmap tilePixmap = QPixmap::fromImage(tileImages[tileChunk]);
                QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
                tilePixmap.setMask(tileMask);
                QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
                tilePixmapItem->setPos(x - tileInfos[tileChunk].x, y - tileInfos[tileChunk].y);
                if (!name.isEmpty()) {
                    QGraphicsTextItem* textItem = m_mapScene->addText(name);
                    textItem->setPos(x - textItem->boundingRect().width() / 2,
                                     y + textItem->boundingRect().height() / 2);
                    textItem->setDefaultTextColor(Qt::red);
                }
                QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
                dot->setBrush(Qt::gray);
            }
        });
    }

    // 绘制上市企业节点 → 收集（Layer 20: 地块 + 立体建筑精灵）
    std::vector<CommercialInfo> commercialInfos = parseCommercialInfos(bytes);
    for (const CommercialInfo& item : commercialInfos) {
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        const int16_t spriteOffset = item.sprite;
        const int face = item.face;
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));

        drawEntries.push_back(DrawEntry{
            20,  // Layer 20 = 立体物体层
            y,
            [this, x, y, tileChunk, spriteOffset, face, name,
             TRANSPARENT, count, MAP_SPRITE_OFFSET, LARGE_TILE_CHUNK_OFFSET,
             resourceModel,
             &tileImages, &tileInfos]() {
                // 地块
                QPixmap tilePixmap = QPixmap::fromImage(tileImages[tileChunk]);
                QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
                tilePixmap.setMask(tileMask);
                QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
                tilePixmapItem->setPos(x - tileInfos[tileChunk].x, y - tileInfos[tileChunk].y);
                // Sprite
                if (spriteOffset <= 0) return;
                const int chunk = offsetChunk8(face, 0, m_northDirection);
                const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
                if (spriteResourceIndex <= 0 || spriteResourceIndex >= resourceModel->n()) return;
                const QString type = resourceModel->getType(spriteResourceIndex);
                if (!type.startsWith("SPR") && !type.startsWith("SMP")) return;
                std::vector<GraphInfo> infos = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
                if (chunk < 0 || chunk >= (int)infos.size()) return;
                std::vector<QImage> images = parseImages(resourceModel->getResource(spriteResourceIndex));
                if (images[chunk].isNull()) return;
                QPixmap pixmap = QPixmap::fromImage(images[chunk]);
                QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                pixmap.setMask(mask);
                QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
                // 名称
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
        });
    }

    // 绘制美观节点 → 收集（Layer 20: 立体装饰精灵）
    std::vector<BeautyInfo> beautyInfos = parseBeautyInfos(bytes);
    for (const BeautyInfo& item : beautyInfos) {
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int16_t spriteOffset = item.sprite;
        const int face = item.face;
        QString name = parseBig5Trim(QByteArray::fromRawData(item.name, sizeof(item.name)));

        drawEntries.push_back(DrawEntry{
            20,  // Layer 20 = 立体物体层
            y,
            [this, x, y, spriteOffset, face, name,
             TRANSPARENT, count, MAP_SPRITE_OFFSET, resourceModel]() {
                if (spriteOffset <= 0) return;
                const int chunk = offsetChunk8(face, 0, m_northDirection);
                const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
                if (spriteResourceIndex <= 0 || spriteResourceIndex >= resourceModel->n()) return;
                const QString type = resourceModel->getType(spriteResourceIndex);
                if (!type.startsWith("SPR") && !type.startsWith("SMP")) return;
                std::vector<GraphInfo> infos = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
                if (chunk < 0 || chunk >= (int)infos.size()) return;
                std::vector<QImage> images = parseImages(resourceModel->getResource(spriteResourceIndex));
                if (images[chunk].isNull()) return;
                QPixmap pixmap = QPixmap::fromImage(images[chunk]);
                QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                pixmap.setMask(mask);
                QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
                if (!name.isEmpty()) {
                    QGraphicsTextItem* textItem = m_mapScene->addText(name);
                    textItem->setPos(x - textItem->boundingRect().width() / 2,
                                     y + textItem->boundingRect().height() / 2);
                    textItem->setDefaultTextColor(Qt::green);
                }
                QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
                dot->setBrush(Qt::green);
            }
        });
    }

    // 先 layer（低→高）、再画布 y（小→大）升序排序 → 画家算法
    std::sort(drawEntries.begin(), drawEntries.end(),
              [](const DrawEntry& a, const DrawEntry& b) {
                  if (a.layer != b.layer) return a.layer < b.layer;
                  return a.sortY < b.sortY;
              });

    // 统一绘制
    for (auto& entry : drawEntries) {
        entry.paint();
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

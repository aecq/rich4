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

    // 先记录当前 map，ensureLoaded 的身份写入 m_loadedIndex/m_loadedModel
    m_mapIndex = mapIndex;
    m_resourceModel = resourceModel;

    // 阶段一：取 bytes + 全部 parse（若身份命中则 zero-cost 直接 return）
    if (!ensureLoaded(mapIndex, resourceModel)) {
        emit mapTextReady(QString());
        return;
    }

    // 阶段二：用 parse 产物 + 当前 north 画 scene
    redrawScene();

    // 首次加载 MAP：重置视图以适配场景，并保证 pivot 对齐到视口中心
    // 适配真实内容（不加 padding 的版本，初始视图更紧凑）
    QRectF contentRect = m_mapScene->itemsBoundingRect();
    if (contentRect.isNull()) {
        contentRect = QRectF(0, 0, kSceneSize, kSceneSize);
    }
    m_mapView->fitInView(contentRect, Qt::KeepAspectRatio);
    m_mapView->centerOn(m_pivotX, m_pivotY);

    QString text = buildMapText(mapIndex, resourceModel);
    emit mapTextReady(text);
}

// ---------------------------------------------------------------------------
// 公共槽：清空（清 scene、节点、parse 产物、身份 key）
// ---------------------------------------------------------------------------
void MapPanelWidget::clear() {
    // 原始保留成员
    m_mapNodes.clear();

    // 身份 + parse 计数器
    m_loadedIndex = QModelIndex();
    m_loadedModel = nullptr;
    // 注意：不清 m_parseCount，作为整个生命周期的累计度量。

    // Raw bytes 与派生 parse 产物
    m_mapBytes.clear();
    m_gndCount = 0;
    m_nodeType.clear();
    m_chunkOffset = 0;
    m_nodeImages.clear();
    m_nodeInfos.clear();
    m_tileImages.clear();
    m_tileInfos.clear();
    m_facilityInfos.clear();
    m_commercialInfos.clear();
    m_beautyInfos.clear();
    m_sprites.clear();

    // 5 组 QString 名字向量
    m_mapNodeNames.clear();
    m_facilityNames.clear();
    m_commercialNames.clear();
    m_beautyNames.clear();
    m_mapNodeTypes.clear();

    // UI 保留
    if (m_mapScene) {
        m_mapScene->clear();
    }
    emit mapTextReady(QString());
    m_mapIndex = QModelIndex();
    m_resourceModel = nullptr;
}

// ---------------------------------------------------------------------------
// North 方向（Ground 旋转的 TopLeftIndex 接口）
// ---------------------------------------------------------------------------
void MapPanelWidget::setNorthDirection(int topLeftIndex) {
    // clamp 到 kTopLeftIndexMin..kTopLeftIndexMax
    if (topLeftIndex < kTopLeftIndexMin) topLeftIndex = kTopLeftIndexMin;
    if (topLeftIndex > kTopLeftIndexMax) topLeftIndex = kTopLeftIndexMax;

    // ===== L1 guard：方向值未变则零工作 return =====
    if (topLeftIndex == m_northDirection) {
        return;
    }
    m_northDirection = topLeftIndex;

    // 只有"已加载地图"时才需要重画 scene；旋转永不 parse
    if (m_mapIndex.isValid() && m_resourceModel != nullptr) {
        // ensureLoaded 在身份命中时直接 return，零成本
        if (ensureLoaded(m_mapIndex, m_resourceModel)) {
            redrawScene();
        }
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

// ===========================================================================
// 两阶段渲染 阶段一：ensureLoaded（身份未变 → 直接 true；否则 parse 全流程）
// ===========================================================================
bool MapPanelWidget::ensureLoaded(const QModelIndex& index, ResourceModel* resourceModel) {
    // ---- 0. 输入无效 → 主动清空身份并 false ----
    if (!index.isValid() || resourceModel == nullptr) {
        // 保守地清掉身份缓存（不清 parseCount 累计）
        m_loadedIndex = QModelIndex();
        m_loadedModel = nullptr;
        return false;
    }

    // ---- 1. 身份命中 → 零 parse 直接 true（这是旋转时的 fast-path） ----
    if (index == m_loadedIndex && resourceModel == m_loadedModel) {
        return true;
    }

    // ---- 2. 身份 miss：开始完整 parse 流程 ----
    ++m_parseCount;

    // 2a. 取 MAP bytes + 解析 MapNode
    m_mapBytes = resourceModel->getResource(index.row());
    m_mapNodes = parseMapNodes(m_mapBytes, 0);

    // 2b. 统计 GND 资源数量（到第一个 SMP/SPR 为止）
    m_gndCount = 0;
    for (int i = 0; i < resourceModel->n(); i++) {
        if (resourceModel->getSignature(i).startsWith("GND")) {
            m_gndCount++;
        } else if (resourceModel->getSignature(i).startsWith("SMP")
                || resourceModel->getSignature(i).startsWith("SPR")) {
            break;
        }
    }

    // 2c. 地图贴地 sprite + 地块 的图元信息
    const int TILE_OFFSET = 2;
    m_nodeImages = parseImages(resourceModel->getResource(3 * m_gndCount));
    m_nodeInfos  = parseGraphInfos(resourceModel->getResource(3 * m_gndCount));
    m_tileInfos  = parseGraphInfos(resourceModel->getResource(3 * m_gndCount + TILE_OFFSET));
    m_tileImages = parseImages(resourceModel->getResource(3 * m_gndCount + TILE_OFFSET));

    // 2d. nodeType / chunkOffset（MapNode.chunk 用）
    m_nodeType    = resourceModel->getType(index.row());
    m_chunkOffset = m_nodeType.mid(3, m_nodeType.length() - 3).toInt();

    // 2e. 3 组 *Infos（每个只 parse 1 次；原 populateScene 里 parseFacilityInfos 等
    //     在 reserve 时各调 1 次 + 真正使用各调 1 次，共 2 次，这里合并成 1 次）
    m_facilityInfos   = parseFacilityInfos(m_mapBytes);
    m_commercialInfos = parseCommercialInfos(m_mapBytes);
    m_beautyInfos     = parseBeautyInfos(m_mapBytes);

    // 2f. 上一次遗留的 sprite 缓存（不同 MAP 身份 → 清理）
    m_sprites.clear();

    // ---- 3. 填充 5 组 QString 名字/类型向量 ----
    const size_t N = m_mapNodes.size();
    m_mapNodeNames.resize(N);
    m_mapNodeTypes.resize(N);
    for (size_t i = 0; i < N; i++) {
        const MapNode& node = m_mapNodes[i];
        m_mapNodeNames[i] = parseBig5Trim(
            QByteArray::fromRawData(node.name, sizeof(node.name)));
        m_mapNodeTypes[i] = QString::number(node.type);
    }

    const size_t F = m_facilityInfos.size();
    m_facilityNames.resize(F);
    for (size_t i = 0; i < F; i++) {
        const FacilityInfo& item = m_facilityInfos[i];
        m_facilityNames[i] = parseBig5Trim(
            QByteArray::fromRawData(item.name, sizeof(item.name)));
    }

    const size_t C = m_commercialInfos.size();
    m_commercialNames.resize(C);
    for (size_t i = 0; i < C; i++) {
        const CommercialInfo& item = m_commercialInfos[i];
        m_commercialNames[i] = parseBig5Trim(
            QByteArray::fromRawData(item.name, sizeof(item.name)));
    }

    const size_t B = m_beautyInfos.size();
    m_beautyNames.resize(B);
    for (size_t i = 0; i < B; i++) {
        const BeautyInfo& item = m_beautyInfos[i];
        m_beautyNames[i] = parseBig5Trim(
            QByteArray::fromRawData(item.name, sizeof(item.name)));
    }

    // ---- 4. 写入身份 key：下一次命中就不用再 parse ----
    m_loadedIndex = index;
    m_loadedModel = resourceModel;
    return true;
}

// ===========================================================================
// 两阶段渲染 阶段二：redrawScene（永不 parse；只依赖 ensureLoaded 产物 + north）
// ===========================================================================
void MapPanelWidget::redrawScene() {
    // 视图 show 兜底（原 populateScene 入口的行为）
    if (m_mapView) m_mapView->show();

    // 清 scene（scene->clear 不会影响已 parse 的成员）
    m_mapScene->clear();

    if (m_mapNodes.empty()
     && m_facilityInfos.empty()
     && m_commercialInfos.empty()
     && m_beautyInfos.empty()) {
        // 没有任何可画内容 → 直接维持兜底 sceneRect
        m_mapScene->setSceneRect(0, 0, kSceneSize, kSceneSize);
        return;
    }

    // 编译期常量（从原 populateScene 里提到函数级，保持原行为）
    const QColor TRANSPARENT(Qt::black);
    const int MAP_SPRITE_OFFSET = 14;
    const int LARGE_TILE_CHUNK_OFFSET = 2;
    QColor colors[5] = { Qt::gray, Qt::gray, Qt::cyan, Qt::cyan, Qt::cyan };
    const int denominator = 2000;
    const int count = m_gndCount;             // 闭包中引用
    ResourceModel* const resourceModel = m_loadedModel;   // redrawScene 里需要取 sprite 资源类型

    // ------------------------------------------------------------------
    // 分层深度排序（画家算法 + 层级）：
    //   Layer  5 = 地块
    //   Layer 10 = 贴地层   → MapNode（地面贴图，永远在立体物体之下）
    //   Layer 20 = 立体物体层 → Facility/Commercial/Beauty（建筑 / 装饰精灵）
    //   Layer 30 = Sign
    // 排序规则：先按 layer 升序（低层先画被高层盖住），同 layer 再按画布 y 升序
    // ------------------------------------------------------------------
    struct DrawEntry {
        int layer;
        float sortY;
        std::function<void()> paint;
    };
    std::vector<DrawEntry> drawEntries;
    drawEntries.reserve(m_mapNodes.size()
                        + m_facilityInfos.size()
                        + m_commercialInfos.size()
                        + m_beautyInfos.size());

    // ---------------------------------------------------------------
    // MapNode 层：Layer 10（贴地）+ Layer 30（Sign/dot/typeStr）
    // ---------------------------------------------------------------
    const size_t N = m_mapNodes.size();
    for (size_t i = 0; i < N; i++) {
        const MapNode& node = m_mapNodes[i];
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(node.x),
                                                        static_cast<float>(node.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;

        const QString& name = m_mapNodeNames[i];
        const QString& typeStr = m_mapNodeTypes[i];
        int chunk = node.chunk + m_chunkOffset;
        const bool hasImage = (node.special > 0 && chunk < (int)m_nodeImages.size() && !m_nodeImages[chunk].isNull())
                           || (node.special <= 0 && 0 < chunk && chunk < (int)m_nodeImages.size());

        // Layer 10：贴地图
        drawEntries.push_back(DrawEntry{
            10,
            y,
            [this, x, y, chunk, hasImage, TRANSPARENT]() {
                if (!hasImage) return;
                QPixmap pixmap = QPixmap::fromImage(m_nodeImages[chunk]);
                QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                pixmap.setMask(mask);
                QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                pixmapItem->setPos(x - m_nodeInfos[chunk].x, y - m_nodeInfos[chunk].y);
            }
        });

        // Layer 30：Sign（名称 / type 数字 / 空小圆点）
        drawEntries.push_back(DrawEntry{
            30,
            y,
            [this, x, y, &name, &typeStr, chunk, node, colors, denominator]() {
                if (!name.isEmpty()) {
                    QGraphicsTextItem* textItem = m_mapScene->addText(name);
                    textItem->setPos(x - textItem->boundingRect().width() / 2,
                                     y + ((node.special > 0) ? m_nodeInfos[chunk].y : 0));
                    textItem->setDefaultTextColor(colors[node.type / denominator]);
                }
                if (node.type != 0) {
                    QGraphicsTextItem* typeItem = m_mapScene->addText(typeStr);
                    typeItem->setPos(x - typeItem->boundingRect().width() / 2,
                                     y - ((node.special > 0) ? m_nodeInfos[chunk].y : 0)
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

    // ---------------------------------------------------------------
    // 设施 Facility：Layer 5（地块） + Layer 30（Sign）
    // ---------------------------------------------------------------
    const size_t F = m_facilityInfos.size();
    for (size_t i = 0; i < F; i++) {
        const FacilityInfo& item = m_facilityInfos[i];
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        const QString& name = m_facilityNames[i];

        drawEntries.push_back(DrawEntry{
            5,
            y,
            [this, x, y, tileChunk, TRANSPARENT]() {
                QPixmap tilePixmap = QPixmap::fromImage(m_tileImages[tileChunk]);
                QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
                tilePixmap.setMask(tileMask);
                QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
                tilePixmapItem->setPos(x - m_tileInfos[tileChunk].x, y - m_tileInfos[tileChunk].y);
            }
        });

        drawEntries.push_back(DrawEntry{
            30,
            y,
            [this, x, y, &name]() {
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

    // ---------------------------------------------------------------
    // 上市企业 Commercial：Layer 5（地块）+ Layer 20（立体建筑精灵）+ Layer 30
    // ---------------------------------------------------------------
    const size_t C = m_commercialInfos.size();
    for (size_t i = 0; i < C; i++) {
        const CommercialInfo& item = m_commercialInfos[i];
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int absTileChunk = LARGE_TILE_CHUNK_OFFSET + (item.face & 1);
        const int tileChunk = offsetChunk2(absTileChunk, LARGE_TILE_CHUNK_OFFSET, m_northDirection);
        const int16_t spriteOffset = item.sprite;
        const int face = item.face;
        const QString& name = m_commercialNames[i];

        // Layer 5：地块
        drawEntries.push_back(DrawEntry{
            5,
            y,
            [this, x, y, tileChunk, TRANSPARENT]() {
                QPixmap tilePixmap = QPixmap::fromImage(m_tileImages[tileChunk]);
                QBitmap tileMask = tilePixmap.createMaskFromColor(TRANSPARENT);
                tilePixmap.setMask(tileMask);
                QGraphicsPixmapItem* tilePixmapItem = m_mapScene->addPixmap(tilePixmap);
                tilePixmapItem->setPos(x - m_tileInfos[tileChunk].x, y - m_tileInfos[tileChunk].y);
            }
        });

        // Layer 20：Sprite 建筑（解析通过 m_sprites 懒加载一次，永不重复 parse 同一 SPR/SMP）
        drawEntries.push_back(DrawEntry{
            20,
            y,
            [this, x, y, spriteOffset, face, TRANSPARENT, count, MAP_SPRITE_OFFSET, resourceModel]() {
                if (spriteOffset <= 0) return;
                const int chunk = offsetChunk8(face, 0, m_northDirection);
                const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
                if (spriteResourceIndex <= 0 || !resourceModel || spriteResourceIndex >= resourceModel->n()) return;
                const QString type = resourceModel->getType(spriteResourceIndex);
                if (!type.startsWith("SPR") && !type.startsWith("SMP")) return;

                auto it = m_sprites.constFind(spriteResourceIndex);
                if (it == m_sprites.constEnd()) {
                    auto infos  = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
                    auto images = parseImages(resourceModel->getResource(spriteResourceIndex));
                    it = m_sprites.insert(spriteResourceIndex,
                                          qMakePair(std::move(infos), std::move(images)));
                }
                const auto& infos  = it.value().first;
                const auto& images = it.value().second;
                if (chunk < 0 || chunk >= (int)infos.size()) return;
                if (images[chunk].isNull()) return;
                QPixmap pixmap = QPixmap::fromImage(images[chunk]);
                QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                pixmap.setMask(mask);
                QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
            }
        });

        // Layer 30：Sign
        drawEntries.push_back(DrawEntry{
            30,
            y,
            [this, x, y, &name]() {
                if (!name.isEmpty()) {
                    QGraphicsTextItem* textItem = m_mapScene->addText(name);
                    textItem->setPos(x - textItem->boundingRect().width() / 2,
                                     y + textItem->boundingRect().height() / 2);
                    textItem->setDefaultTextColor(Qt::blue);
                }
                QGraphicsEllipseItem* dot = m_mapScene->addEllipse(x - 3, y - 3, 6, 6);
                dot->setBrush(Qt::gray);
            }
        });
    }

    // ---------------------------------------------------------------
    // 美观 Beauty：Layer 20（装饰精灵）+ Layer 30（Sign）
    // ---------------------------------------------------------------
    const size_t B = m_beautyInfos.size();
    for (size_t i = 0; i < B; i++) {
        const BeautyInfo& item = m_beautyInfos[i];
        std::pair<float, float> canvasXY = rotateAround(static_cast<float>(item.x),
                                                        static_cast<float>(item.y),
                                                        m_northDirection);
        const float x = canvasXY.first;
        const float y = canvasXY.second;
        const int16_t spriteOffset = item.sprite;
        const int face = item.face;
        const QString& name = m_beautyNames[i];

        drawEntries.push_back(DrawEntry{
            20,
            y,
            [this, x, y, spriteOffset, face, name, TRANSPARENT, count, MAP_SPRITE_OFFSET, resourceModel]() {
                Q_UNUSED(name);
                if (spriteOffset <= 0) return;
                const int chunk = offsetChunk8(face, 0, m_northDirection);
                const int spriteResourceIndex = 3 * count + MAP_SPRITE_OFFSET + spriteOffset;
                if (spriteResourceIndex <= 0 || !resourceModel || spriteResourceIndex >= resourceModel->n()) return;
                const QString type = resourceModel->getType(spriteResourceIndex);
                if (!type.startsWith("SPR") && !type.startsWith("SMP")) return;

                auto it = m_sprites.constFind(spriteResourceIndex);
                if (it == m_sprites.constEnd()) {
                    auto infos  = parseGraphInfos(resourceModel->getResource(spriteResourceIndex));
                    auto images = parseImages(resourceModel->getResource(spriteResourceIndex));
                    it = m_sprites.insert(spriteResourceIndex,
                                          qMakePair(std::move(infos), std::move(images)));
                }
                const auto& infos  = it.value().first;
                const auto& images = it.value().second;
                if (chunk < 0 || chunk >= (int)infos.size()) return;
                if (images[chunk].isNull()) return;
                QPixmap pixmap = QPixmap::fromImage(images[chunk]);
                QBitmap mask = pixmap.createMaskFromColor(TRANSPARENT);
                pixmap.setMask(mask);
                QGraphicsPixmapItem* pixmapItem = m_mapScene->addPixmap(pixmap);
                pixmapItem->setPos(x - infos[chunk].x, y - infos[chunk].y);
            }
        });

        drawEntries.push_back(DrawEntry{
            30,
            y,
            [this, x, y, &name]() {
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
        contentRect = QRectF(0, 0, kSceneSize, kSceneSize);
    }
    const qreal padding = 200.0;
    contentRect.adjust(-padding, -padding, padding, padding);
    contentRect = contentRect.united(QRectF(m_pivotX, m_pivotY, 1.0, 1.0));
    m_mapScene->setSceneRect(contentRect);
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
    Q_ASSERT(S > 0);
    const int k = absChunk - base;
    int kNew = (k - i) % S;
    if (kNew < 0) {
        kNew += S;
    }
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
// 鼠标位置：转发到状态栏（显示 Canvas XY + 反变换后的 Map XY）
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
    const std::pair<float, float> G_geo =
        rotateAround(static_cast<float>(spPrev.x()),
                     static_cast<float>(spPrev.y()),
                     1 - T_old);

    // ================================================================
    // 阶段 2：执行 TopLeftIndex 切换（环形 0..7）。
    //   setNorthDirection 内部若方向真的变了 → ensureLoaded(identity-hit) + redrawScene，
    //   否则直接 no-op（parseCount 不动）。
    // ================================================================
    const int S = kTopLeftIndexMax - kTopLeftIndexMin + 1;
    int next = T_old + delta;
    next = ((next % S) + S) % S;
    setNorthDirection(next);

    // ================================================================
    // 阶段 3：旋转后，把 G_geo 在新朝向下的画布坐标"搬回"视口像素 Vp。
    // ================================================================
    const int T_new = m_northDirection;
    const std::pair<float, float> S_next =
        rotateAround(G_geo.first, G_geo.second, T_new);
    const QPointF spNext(static_cast<qreal>(S_next.first),
                         static_cast<qreal>(S_next.second));

    const QTransform M = m_mapView->transform();
    const qreal sx = (qFuzzyIsNull(M.m11())) ? 1.0 : 1.0 / M.m11();
    const qreal sy = (qFuzzyIsNull(M.m22())) ? 1.0 : 1.0 / M.m22();

    const QPoint offsetPx = Vp - vpCenter;
    const QPointF targetCenter(spNext.x() - offsetPx.x() * sx,
                               spNext.y() - offsetPx.y() * sy);

    m_mapView->centerOn(targetCenter);
}

// ===========================================================================
// 兼容包装：populateScene（存在调用点的老代码路径仍可用）
//   语义等价于：ensureLoaded + redrawScene
// ===========================================================================
void MapPanelWidget::populateScene(const QModelIndex& index, ResourceModel* resourceModel) {
    if (!ensureLoaded(index, resourceModel)) return;
    redrawScene();
}

// ===========================================================================
// 私有：生成右侧节点列表文本
//   优先使用 ensureLoaded 阶段预转好的 m_mapNodeNames；
//   若 sizes 对不上（理论不会）则回退到原 parseBig5Trim 行为做安全兜底。
// ===========================================================================
QString MapPanelWidget::buildMapText(const QModelIndex& index, ResourceModel* resourceModel) {
    QString text;
    text += resourceModel->getType(index.row());
    text += "\nScale: CTRL + Wheel";
    text += QString("\nMap Node Count: %1").arg(m_mapNodes.size()) + "\n";

    const bool haveNames = (m_mapNodeNames.size() == m_mapNodes.size())
                        && (m_mapNodeTypes.size() == m_mapNodes.size());
    for (int i = 0; i < (int)m_mapNodes.size(); i++) {
        const QString& name = haveNames
            ? m_mapNodeNames[i]
            : parseBig5Trim(QByteArray::fromRawData(
                  m_mapNodes[i].name, sizeof(MapNode::name)));
        const QString& typeStr = haveNames
            ? m_mapNodeTypes[i]
            : QString::number(m_mapNodes[i].type);
        text += QString("\n%1 (%2, %3) %4: %5")
            .arg(i, 3, 10, QChar(' '))
            .arg(m_mapNodes[i].x)
            .arg(m_mapNodes[i].y)
            .arg(typeStr, 4, QLatin1Char(' '))
            .arg(name);
        text += QString(" %1 %2 %3 %4\n")
            .arg(m_mapNodes[i].neighbors[0], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[1], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[2], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[3], 2, 10, QChar(' '));
    }
    return text;
}

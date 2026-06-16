#include "gui/graphics_text_window.h"
#include "core/io/parse.h"
#include "gui/main_window.h"
#include <QFileDialog>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWheelEvent>

GraphicsTextWindow::GraphicsTextWindow(MainWindow* mainWindow) : QWidget(nullptr, Qt::Window), m_mainWindow(mainWindow) {
    setupUI();
    resize(1000, 600);
}

GraphicsTextWindow::~GraphicsTextWindow() {
}

void GraphicsTextWindow::setupUI() {
    setWindowTitle("Graphics Text Window");

    // 1. 创建主布局（让分割器铺满整个窗口）
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); // 去掉边距
    mainLayout->setSpacing(0);

    // 2. 水平分割器：左右两栏
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(mainSplitter); // 把分割器放进主布局

    // 3. 左右面板
    QWidget* leftPanel = new QWidget(mainSplitter);
    QWidget* rightPanel = new QWidget(mainSplitter);

    // 4. 给左右面板各自设置布局（必须加，否则控件不会铺满）
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    // 5. 右侧：只读文本框
    textEdit = new QTextEdit(rightPanel);
    textEdit->setReadOnly(true);
    textEdit->setPlainText("Right Panel Text");
    textEdit->setFont(QFont("Microsoft YaHei", 14));
    rightLayout->addWidget(textEdit); // 让编辑框铺满右侧

    // 6. 左侧：你可以放图形视图、按钮、列表等
    gallery = new QListWidget(leftPanel);
    gallery->setViewMode(QListWidget::IconMode);
    gallery->setIconSize(QSize(1280, 960));
    leftLayout->addWidget(gallery);
    // 在 gallery 创建后添加
    mapView = new MapGraphicsView(leftPanel);
    mapScene = new QGraphicsScene(mapView);
    mapView->setScene(mapScene);
    mapView->setDragMode(QGraphicsView::ScrollHandDrag);     // 拖拽平移
    mapView->setInteractive(true);
    mapView->setRenderHint(QPainter::Antialiasing);
    mapView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    mapView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    mapView->hide();
    leftLayout->addWidget(mapView);

    // 7. 分割器初始宽度
    mainSplitter->setSizes({1500, 500});

    // 8. 设置右键菜单
    gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gallery, &QListWidget::customContextMenuRequested,
            this, &GraphicsTextWindow::onGalleryContextMenu);

    // 添加鼠标指针 Map Scene XY 坐标显示连接
    connect(mapView, &MapGraphicsView::mousePositionChanged,
            this, &GraphicsTextWindow::onMousePositionChanged);
}

void GraphicsTextWindow::update(const QModelIndex &index) {
    int depth = indexDepth(index);
    ResourceModel* resourceModel = m_mainWindow->getResourceModel();
    // Image
    if (mapView) mapView->hide();
    gallery->show();
    gallery->clear();
    m_images.clear();
    if (depth == 1) {
        QString sig = resourceModel->getSignature(index.row());
        QString type = resourceModel->getType(index.row());
        if (sig.startsWith("SPR") || sig.startsWith("SMP")) {
            m_images = parseImages(resourceModel->getResource(index.row()));
            displayImages();
        } else if (type.startsWith("!")) {
            displayRawImage(index, resourceModel, false);
        } else if (type.startsWith("$")) {
            displayRawImage(index, resourceModel, true);
        } else if (type.startsWith("MAP")) {
            displayMap(index, resourceModel);
        } else if (type.startsWith("FLC")) {
            m_images = parseFLIC(resourceModel->getResource(index.row()));
            displayImages();
        }
    } else if (depth == 2) {
        QModelIndex parent = index.parent();
        QString sig = resourceModel->getSignature(parent.row());
        if ((sig.startsWith("SPR") || sig.startsWith("SMP"))) {
            std::vector<QImage> images = parseImages(resourceModel->getResource(parent.row()));
            if (index.row() < images.size()) {
                m_images.push_back(images[index.row()]);
                displayImages();
            }
        }
    }
    // Text
    if (depth == 1) {
        QString sig = resourceModel->getSignature(index.row());
        QString type = resourceModel->getType(index.row());
        if (sig.startsWith("SPR")) {
            textEdit->setHtml(paletteHTML(index, resourceModel));
        } else if (sig.startsWith("SMP") || sig.startsWith("RIFF")) {
            textEdit->setPlainText(sig);
        } else if (type.startsWith("MAP")) {
            displayMapText(index, resourceModel);
        } else {
            textEdit->setPlainText(
                parseBig5(resourceModel->getResource(index.row()).left(2 * 1024)));
        }
    } else if (depth == 2) {
        QModelIndex parent = index.parent();
        QString sig = resourceModel->getSignature(parent.row());
        if (sig.startsWith("SPR")) {
            textEdit->setHtml(paletteHTML(parent, resourceModel));
        } else {
            textEdit->setPlainText("");
        }
    }
}

QString GraphicsTextWindow::paletteHTML(const QModelIndex &index, ResourceModel* resourceModel) {
    QByteArray bytes = resourceModel->getResource(index.row());
    SPRSMPHeader header = parseSPRSMPHeader(bytes);
    QVector<QRgb> palette = parsePalette(bytes, header.start_offset);
    QString text = "";
    for (int i = 0; i < palette.size(); i++) {
        QRgb c = palette[i];
        text += QString("<font color=\"#%1%2%3\">█</font>")
            .arg(qRed(c), 2, 16, QChar('0'))
            .arg(qGreen(c), 2, 16, QChar('0'))
            .arg(qBlue(c), 2, 16, QChar('0'));
        if ((i+1) % 16 == 0) {
            text += "<br>";
        }
    }
    return text;
}

void GraphicsTextWindow::displayMap(const QModelIndex& index, ResourceModel* resourceModel) {
    // 切换显示模式
    gallery->hide();
    mapView->show();

    // 解析节点
    QByteArray bytes = resourceModel->getResource(index.row());
    m_mapNodes = parseMapNodes(bytes, 0);
    int count = 0;
    for (int i = 0; i < resourceModel->n(); i++) {
        if (resourceModel->getSignature(i).startsWith("GND")) {
            count++;
        } else if (resourceModel->getSignature(i).startsWith("SMP") || resourceModel->getSignature(i).startsWith("SPR")) {
            break;
        }
    }
    std::vector<QImage> images = parseImages(resourceModel->getResource(3 * count));

    mapScene->clear();

    if (m_mapNodes.empty()) return;

    // // 计算缩放因子和场景范围
    // int margin = 50;
    int sceneSize = 2300;  // 场景大小

    float factor = 1.0f * mapView->width() / sceneSize;
    mapView->scale(factor, factor);

    // 创建每个节点的 item
    for (size_t i = 0; i < m_mapNodes.size(); i++) {
        const MapNode& node = m_mapNodes[i];

        // 坐标转换
        float x = node.x;
        float y = node.y;

        // 解析名称
        QString name = parseBig5Trim(QByteArray::fromRawData(node.name, sizeof(node.name)));

        // 添加图片 item
        QString type = resourceModel->getType(index.row());
        int chunkOffset = type.mid(3, type.length() - 3).toInt();
        int chunk = node.chunk + chunkOffset;
        if ((node.special > 0 && chunk < images.size() && !images[chunk].isNull())  // 特殊节点
            || (node.special <= 0 && 0 < chunk && chunk < images.size())  // 非特殊节点有图片
        ) {
            QPixmap pixmap = QPixmap::fromImage(images[chunk]);
            QBitmap mask = pixmap.createMaskFromColor(Qt::black);
            pixmap.setMask(mask);
            QGraphicsPixmapItem* pixmapItem = mapScene->addPixmap(pixmap);
            pixmapItem->setPos(x - images[chunk].width() / 2, y - images[chunk].height() / 2);
        }

        // 创建文本 item
        QColor colors[5] = {Qt::gray, Qt::gray, Qt::cyan, Qt::cyan, Qt::cyan};
        int denominator = 2000;
        if (name.length() > 0) {
            QGraphicsTextItem* textItem = mapScene->addText(name);
            textItem->setPos(x - textItem->boundingRect().width() / 2, y + ((node.special > 0) ? images[chunk].height() / 2 : 0));
            textItem->setDefaultTextColor(node.special > 0 ? Qt::darkCyan : Qt::gray);
            textItem->setDefaultTextColor(colors[node.type / denominator]);
        }

        if (node.type != 0) {
            QString typeStr = QString::number(node.type);
            QGraphicsTextItem* typeItem = mapScene->addText(typeStr);
            typeItem->setPos(x - typeItem->boundingRect().width() / 2, y - ((node.special > 0) ? images[chunk].height() / 2 : 0) - typeItem->boundingRect().height());
            typeItem->setDefaultTextColor(colors[node.type / denominator]);
        }

        // 非特殊节点没有图片时添加点标记
        if (node.special <= 0 && chunk <= 0) {
            QGraphicsEllipseItem* dot = mapScene->addEllipse(x-3, y-3, 6, 6);
            dot->setBrush(Qt::gray);
        }
    }

    mapScene->setSceneRect(0, 0, sceneSize, sceneSize);
    mapView->fitInView(mapScene->sceneRect(), Qt::KeepAspectRatio);
}

void GraphicsTextWindow::displayMapText(const QModelIndex& index, ResourceModel* resourceModel) {
    QString text;
    text += resourceModel->getType(index.row());
    text += "\nScale: CTRL + Wheel";
    text += QString("\nMap Node Count: %1").arg(m_mapNodes.size()) + "\n";
    for (int i = 0; i < m_mapNodes.size(); i++) {
        text += QString("\n%1 (%2, %3) %4: %5")
            .arg(i, 3, 10, QChar(' '))
            .arg(m_mapNodes[i].x)
            .arg(m_mapNodes[i].y)
            .arg(m_mapNodes[i].type, 4, 10, QChar(' '))
            .arg(parseBig5Trim(QByteArray::fromRawData(m_mapNodes[i].name, sizeof(MapNode::name))));
        text += QString(" %1 %2 %3 %4\n")
            .arg(m_mapNodes[i].neighbors[0], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[1], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[2], 2, 10, QChar(' '))
            .arg(m_mapNodes[i].neighbors[3], 2, 10, QChar(' '));
    }
    textEdit->setPlainText(text);
}

void GraphicsTextWindow::onTreeRowChanged(const QModelIndex &index) {
    gallery->clear();
    textEdit->clear();
    if (!index.isValid()) {
        return;
    }
    update(index);
}

void GraphicsTextWindow::displayRawImage(const QModelIndex& index, ResourceModel* resourceModel, bool isGrayscale) {
    QString type = resourceModel->getType(index.row());
    auto wxh = parseWXH(type);
    int width = wxh.first.width;
    int height = wxh.first.height;
    QString msg = wxh.second;
    if (!msg.isEmpty()) {
        emit statusMessage(msg);
        return;
    }
    QByteArray bytes = resourceModel->getResource(index.row());
    uint expectedSize = isGrayscale ? width * height : width * height * 2;
    if (bytes.size() != expectedSize) {
        QString message = QString("%1 x %2 %3 != %4").arg(width).arg(height).arg(isGrayscale ? "" : "x 2").arg(bytes.size());
        emit statusMessage(message);
        return;
    }

    QImage image = parseImage(bytes, width, height,
        isGrayscale ? QImage::Format_Grayscale8 : QImage::Format_RGB555, isGrayscale);
    m_images.push_back(image);
    displayImages();
}

void GraphicsTextWindow::displayImages() {
    for (size_t i = 0; i < m_images.size(); i++) {
        if (m_images[i].isNull()) {
            continue;
        }
        QPixmap pixmap = QPixmap::fromImage(m_images[i]);
        QIcon icon(pixmap);
        QListWidgetItem* item = new QListWidgetItem(
            icon,
            QString("%1: %2x%3")
                .arg(i)
                .arg(m_images[i].width())
                .arg(m_images[i].height()),
            gallery
        );
        item->setTextAlignment(Qt::AlignCenter);
        gallery->addItem(item);
    }
}

void GraphicsTextWindow::onGalleryContextMenu(const QPoint& pos) {
    QListWidgetItem* item = gallery->itemAt(pos);
    if (!item) return;

    int index = gallery->row(item);
    if (index < 0 || index >= m_images.size()) return;

    QMenu menu(this);
    QAction* exportAction = menu.addAction("Export Image");
    QAction* selected = menu.exec(gallery->mapToGlobal(pos));

    if (selected == exportAction) {
        ResourceModel* resourceModel = m_mainWindow->getResourceModel();
        QString basename = resourceModel->getBasename();
        QModelIndex resourceIndex = m_mainWindow->getTreeView()->currentIndex();
        int resourceRow = -1;
        int chunkRow = index;
        int depth = indexDepth(resourceIndex);
        if (depth == 1) {
            resourceRow = resourceIndex.row();
        } else if (depth == 2) {
            QModelIndex parent = resourceIndex.parent();
            resourceRow = parent.row();
            chunkRow = resourceIndex.row();
        }
        QString fileName = QFileDialog::getSaveFileName(
            this,
            "Save Image",
            QString("%1%2-%3.bmp").arg(basename)
                .arg(resourceRow, 4, 10, QChar('0')).arg(chunkRow, 3, 10, QChar('0')),
            "BMP Files (*.bmp);;PNG Files (*.png);;JPEG Files (*.jpg)"
        );

        if (!fileName.isEmpty()) {
            QImage image = m_images[index];
            image.save(fileName);
        }
    }
}

void GraphicsTextWindow::onMousePositionChanged(int x, int y) {
    QString text = QString("Map Scene XY: %1, %2").arg(x).arg(y);
    emit statusMessage(text);
}
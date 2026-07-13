#include "gui/graphics_text_window.h"
#include "core/io/parse.h"
#include "core/io/parse_flic.h"
#include "core/types/ground.h"
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

    // 6. 左侧：gallery（图片列表） + mapPanel（地图视图，互斥显示）
    gallery = new QListWidget(leftPanel);
    gallery->setViewMode(QListWidget::IconMode);
    gallery->setIconSize(QSize(2560, 1920));
    leftLayout->addWidget(gallery);

    mapPanel = new MapPanelWidget(leftPanel);
    mapPanel->hide();
    leftLayout->addWidget(mapPanel);

    // 7. 分割器初始宽度
    mainSplitter->setSizes({1500, 500});

    // 8. 设置右键菜单
    gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gallery, &QListWidget::customContextMenuRequested,
            this, &GraphicsTextWindow::onGalleryContextMenu);

    // 9. MapPanel 信号：文本输出到右栏、状态栏消息转发
    connect(mapPanel, &MapPanelWidget::mapTextReady,
            this, &GraphicsTextWindow::onMapTextReady);
    connect(mapPanel, &MapPanelWidget::statusMessage,
            this, &GraphicsTextWindow::statusMessage);
}

void GraphicsTextWindow::update(const QModelIndex &index) {
    int depth = indexDepth(index);
    ResourceModel* resourceModel = m_mainWindow->getResourceModel();
    // Image / Map：先默认切到 gallery 模式
    mapPanel->clear();
    mapPanel->hide();
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
        } else if (type.startsWith("GND")) {
            Ground ground = parseGround(resourceModel->getResource(index.row()));
            m_images.push_back(ground.stitchFull());
            displayImages();
        } else if (type.startsWith("MAP")) {
            // MAP 模式：切换到 mapPanel，文本由 mapTextReady 信号异步填充
            gallery->hide();
            mapPanel->show();
            mapPanel->loadMap(index, resourceModel);
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
            // MAP 文本由 mapPanel 的 mapTextReady 信号异步填充，无需在这里写 textEdit
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

void GraphicsTextWindow::onTreeRowChanged(const QModelIndex &index) {
    gallery->clear();
    textEdit->clear();
    mapPanel->clear();
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

void GraphicsTextWindow::onMapTextReady(const QString& text) {
    textEdit->setPlainText(text);
}
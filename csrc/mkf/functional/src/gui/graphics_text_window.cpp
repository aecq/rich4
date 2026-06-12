#include "gui/graphics_text_window.h"
#include "core/io/parse.h"
#include "gui/main_window.h"
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>

GraphicsTextWindow::GraphicsTextWindow(MainWindow* mainWindow) : QWidget(mainWindow, Qt::Window), m_mainWindow(mainWindow) {
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

    // 7. 分割器初始宽度
    mainSplitter->setSizes({1500, 500});

    // 8. 设置右键菜单
    gallery->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gallery, &QListWidget::customContextMenuRequested,
            this, &GraphicsTextWindow::onGalleryContextMenu);
}

void GraphicsTextWindow::update(const QModelIndex &index) {
    int depth = indexDepth(index);
    ResourceModel* resourceModel = m_mainWindow->getResourceModel();
    // Image
    gallery->clear();
    m_images.clear();
    if (depth == 1) {
        QString sig = resourceModel->getSignature(index.row());
        QString type = resourceModel->getType(index.row());
        if (sig.startsWith("SPR") || sig.startsWith("SMP")) {
            std::vector<QImage> images = parseImages(resourceModel->getResource(index.row()));
            m_images = images;
            for (size_t i = 0; i < images.size(); i++) {
                if (!images[i].isNull()) {
                    QPixmap pixmap = QPixmap::fromImage(images[i]);
                    QIcon icon(pixmap);
                    QListWidgetItem* item = new QListWidgetItem(
                        icon,
                        QString("%1: %2x%3")
                            .arg(i)
                            .arg(images[i].width())
                            .arg(images[i].height()),
                        gallery
                    );
                    item->setTextAlignment(Qt::AlignCenter);
                    gallery->addItem(item);
                }
            }
        } else if (type.startsWith("!")) {
            int lenType = type.length();
            int indexOfX = type.indexOf("x");
            if (indexOfX == -1) {
                qDebug() << "indexOfX == -1";
                return;
            }
            QString widthStr = type.right(lenType - 1).left(indexOfX - 1);
            QString heightStr = type.right(lenType - indexOfX - 1);
            uint width = widthStr.toUInt();
            uint height = heightStr.toUInt();
            if (width <= 0 || height <= 0) {
                QString message = QString("width(%1) height(%2) is invalid").arg(width).arg(height);
                emit statusMessage(message);
                return;
            }
            QByteArray bytes = resourceModel->getResource(index.row());
            if (width * height * 2 != bytes.size()) {
                QString message =QString("%1 x %2 x 2 != %3. width * height * 2 != bytes.size()").arg(width).arg(height).arg(bytes.size());
                emit statusMessage(message);
                return;
            }
            QImage image = parseImage(bytes, width, height);
            m_images.push_back(image);
            if (!image.isNull()) {
                QPixmap pixmap = QPixmap::fromImage(image);
                QIcon icon(pixmap);
                QListWidgetItem* item = new QListWidgetItem(
                    icon,
                    QString("%1: %2x%3")
                        .arg(index.row())
                        .arg(width)
                        .arg(height),
                    gallery
                );
                item->setTextAlignment(Qt::AlignCenter);
                gallery->addItem(item);
            }
        } else if (type.startsWith("$")) {
            // #region repeated code TODO: refactor this code
            int lenType = type.length();
            int indexOfX = type.indexOf("x");
            if (indexOfX == -1) {
                qDebug() << "indexOfX == -1";
                return;
            }
            QString widthStr = type.right(lenType - 1).left(indexOfX - 1);
            QString heightStr = type.right(lenType - indexOfX - 1);
            uint width = widthStr.toUInt();
            uint height = heightStr.toUInt();
            if (width <= 0 || height <= 0) {
                QString message = QString("width(%1) height(%2) is invalid").arg(width).arg(height);
                emit statusMessage(message);
                return;
            }
            // #endregion repeated code
            QByteArray bytes = resourceModel->getResource(index.row());
            if (width * height != bytes.size()) {
                QString message =QString("%1 x %2 != %3. width * height != bytes.size()").arg(width).arg(height).arg(bytes.size());
                emit statusMessage(message);
                return;
            }
            QImage image = parseImage(bytes, width, height, QImage::Format_Grayscale8, true);
            m_images.push_back(image);
            if (!image.isNull()) {
                QPixmap pixmap = QPixmap::fromImage(image);
                QIcon icon(pixmap);
                QListWidgetItem* item = new QListWidgetItem(
                    icon,
                    QString("%1: %2x%3")
                        .arg(index.row())
                        .arg(width)
                        .arg(height),
                    gallery
                );
                item->setTextAlignment(Qt::AlignCenter);
                gallery->addItem(item);
            }
        } else if (type.startsWith("MAP")) {
            QByteArray bytes = resourceModel->getResource(index.row());
            QImage image = drawMapNodeImage(bytes);
            gallery->addItem(new QListWidgetItem(
                QIcon(QPixmap::fromImage(image)),
                QString("Map %1").arg(index.row()),
                gallery
            ));
        }
    } else if (depth == 2) {
        QModelIndex parent = index.parent();
        QString sig = resourceModel->getSignature(parent.row());
        if ((sig.startsWith("SPR") || sig.startsWith("SMP"))) {
            std::vector<QImage> images = parseImages(resourceModel->getResource(parent.row()));
            m_images = images;
            if (index.row() < images.size() && !images[index.row()].isNull()) {
                QPixmap pixmap = QPixmap::fromImage(images[index.row()]);
                QIcon icon(pixmap);
                QListWidgetItem* item = new QListWidgetItem(
                    icon,
                    QString("%1: %2x%3")
                        .arg(index.row())
                        .arg(images[index.row()].width())
                        .arg(images[index.row()].height()),
                    gallery
                );
                item->setTextAlignment(Qt::AlignCenter);
                gallery->addItem(item);
            }
        }
    }
    // Text
    if (depth == 1) {
        QString sig = resourceModel->getSignature(index.row());
        if (sig.startsWith("SPR")) {
            textEdit->setHtml(paletteHTML(index, resourceModel));
        } else if (sig.startsWith("SMP") || sig.startsWith("RIFF")) {
            textEdit->setPlainText(sig);
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
    if (!index.isValid()) {
        return;
    }
    update(index);
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

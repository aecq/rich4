#include "core/io/Parse.h"
#include "gui/main_window.h"
#include "gui/graphics_text_window.h"
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QTextEdit>
#include <QVBoxLayout>
#include <qabstractitemmodel.h>

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
}

void GraphicsTextWindow::update(const QModelIndex &index) {
    int depth = 0;
    QModelIndex temp = index;
    while (temp.parent().isValid()) {
        temp = temp.parent();
        depth++;
    }
    Cache* cache = m_mainWindow->getCache();
    // Image
    gallery->clear();
    if (depth == 1) {
        QString sig = cache->getSignature(index.row());
        if (sig.startsWith("SPR") || sig.startsWith("SMP")) {
            std::vector<QImage> images = parseImages(cache->getResource(index.row()));
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
        }
    } else if (depth == 2) {
        QModelIndex parent = index.parent();
        QString sig = cache->getSignature(parent.row());
        if ((sig.startsWith("SPR") || sig.startsWith("SMP"))) {
            std::vector<QImage> images = parseImages(cache->getResource(parent.row()));
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
        QString sig = cache->getSignature(index.row());
        if (sig.startsWith("SPR")) {
            textEdit->setHtml(paletteHTML(index, cache));
        } else if (sig.startsWith("SMP") || sig.startsWith("RIFF")) {
            textEdit->setPlainText(sig);
        } else {
            textEdit->setPlainText(
                parseBig5(cache->getResource(index.row()).left(2 * 1024)));
        }
    } else if (depth == 2) {
        QModelIndex parent = index.parent();
        QString sig = cache->getSignature(parent.row());
        if (sig.startsWith("SPR")) {
            textEdit->setHtml(paletteHTML(parent, cache));
        } else {
            textEdit->setPlainText("");
        }
    }
}

QString GraphicsTextWindow::paletteHTML(const QModelIndex &index, Cache* cache) {
    QByteArray bytes = cache->getResource(index.row());
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

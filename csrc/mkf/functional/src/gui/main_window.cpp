#include "gui/main_window.h"
#include "core/io/Parse.h"
#include "core/types/ResourceHeader.h"
#include "core/types/SPRSMPHeader.h"
#include "core/utils/Cache.h"
#include "gui/main_window.h"
#include <QApplication>
#include <QSplitter>
#include <QTreeView>
#include <QTextEdit>
#include <QTableView>
#include <QImage>
#include <QPixmap>
#include <QLabel>
#include <QScrollArea>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenuBar>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QToolBar>
#include <qlogging.h>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    setWindowTitle("MKF File Viewer");
    resize(500, 600);
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    treeView = new QTreeView(this);
    treeModel = new QStandardItemModel(this);
    treeView->setModel(treeModel);

    setCentralWidget(treeView);
    
    createMenuBar();
    createToolBar();
    statusBar()->showMessage("Ready");
}

void MainWindow::createMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    
    QAction* openAction = fileMenu->addAction("&Open MKF...");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
}

void MainWindow::createToolBar() {
    QToolBar* toolBar = addToolBar("Main");
    
    QAction* openAction = toolBar->addAction("Open");
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
}

void MainWindow::openFile() {
    QString filepath = QFileDialog::getOpenFileName(this, "Open MKF File", "", "MKF Files (*.mkf)");
    if (filepath.isEmpty()) {
        return;
    }
    if (!cache)
        cache = new Cache(filepath);
    else
        cache->init(filepath);
    loadFileTree();
    statusBar()->showMessage(QString("Loaded %1 resource(s): %2").arg(cache->n()).arg(filepath));
}

void MainWindow::loadFileTree() {
    treeModel->clear();
    treeModel->setHorizontalHeaderLabels({
        "index",
        "signature",
        "(un)compressed size",
    });

    // 根节点只在第 0 列表显示文件名，其他列留空
    QList<QStandardItem*> rootRow;
    rootRow << new QStandardItem(cache->getFilename().split('/').last())
            << new QStandardItem("")
            << new QStandardItem("");
    treeModel->appendRow(rootRow);
    QStandardItem *rootItem = rootRow[0]; // 取根节点

    for (int i = 0; i < cache->n(); i++) {
        // 从 cache 取数据
        QString sig = cache->getSignature(i);
        uint32_t uncompressed = cache->getHeader(i).uncompressed_size;
        uint32_t compressed = cache->getHeader(i).compressed_size;

        // 创建一行 4 个单元格
        QList<QStandardItem*> row;

        if (sig.startsWith("SPR") || sig.startsWith("SMP")) {
            QByteArray bytes;
            cache->getResource(i, bytes);
            SPRSMPHeader header = parseSPRSMPHeader(bytes);
            row << new QStandardItem(QString::number(i))
                << new QStandardItem(QString("%1 (%2)")
                    .arg(sig).arg(QString::number(header.num_chunks)))
                << new QStandardItem(QString("%1 > %2")
                    .arg(QString::number(uncompressed)).arg(QString::number(compressed)));
            QStandardItem *rowItem = row[0];
            std::vector<GraphInfo> graphInfos = parseGraphInfos(bytes);
            for (int j = 0; j < graphInfos.size(); j++) {
                QList<QStandardItem*> chunkRow;
                chunkRow << new QStandardItem(QString::number(j))
                         << new QStandardItem(QString("%1 x %2")
                            .arg(QString::number(graphInfos[j].width)).arg(QString::number(graphInfos[j].height)))
                         << new QStandardItem(QString("(%1, %2)")
                            .arg(QString::number(graphInfos[j].x)).arg(QString::number(graphInfos[j].y)));
                rowItem->appendRow(chunkRow);
            }
        } else {
            row << new QStandardItem(QString::number(i))
                << new QStandardItem(sig)
                << new QStandardItem(QString("%1 > %2").arg(QString::number(uncompressed)).arg(QString::number(compressed)));
        }
        rootItem->appendRow(row);
    }
    treeView->expandToDepth(0);
}
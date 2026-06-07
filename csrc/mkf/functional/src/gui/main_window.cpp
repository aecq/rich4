#include "core/io/Parse.h"
#include "core/types/ResourceHeader.h"
#include "core/types/SPRSMPHeader.h"
#include "core/utils/Cache.h"
#include "core/utils/MediaPlayer.h"
#include "gui/graphics_text_window.h"
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
#include <qitemselectionmodel.h>
#include <qlogging.h>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUI();
    setupConnections();
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

    QMenu* showMenu = menuBar()->addMenu("&Show");

    playAudioAction = showMenu->addAction("Play Audio");
    playAudioAction->setShortcut(QKeySequence::Refresh);
    connect(playAudioAction, &QAction::triggered, this, &MainWindow::playAudio);
    playAudioAction->setEnabled(false);

    QAction* graphicsTextAction = showMenu->addAction("Graphics Text");
    connect(graphicsTextAction, &QAction::triggered, this, &MainWindow::openGraphicsTextWindow);
}

void MainWindow::createToolBar() {
    QToolBar* toolBar = addToolBar("Main");
    
    QAction* openAction = toolBar->addAction("Open");
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    toolBar->addSeparator();

    toolBar->addAction(playAudioAction);

    QAction* graphicsTextAction = toolBar->addAction("Graphics Text");
    connect(graphicsTextAction, &QAction::triggered, this, &MainWindow::openGraphicsTextWindow);
}

void MainWindow::setupConnections() {
    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::treeSelectionChanged);
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

void MainWindow::playAudio() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }
    // #region depth
    int depth = 0;
    QModelIndex temp = index;
    while (temp.parent().isValid()) {
        temp = temp.parent();
        depth++;
    }
    // #endregion depth
    if (depth == 1 && cache->getSignature(index.row()).startsWith("RIFF")) {
        statusBar()->showMessage("Playing audio at index:" + QString::number(index.row()));
        QByteArray data = cache->getResource(index.row());
        MediaPlayer::instance().play(data);
    } else {
        statusBar()->showMessage("Please select an audio resource.");
    }
}

void MainWindow::openGraphicsTextWindow()
{
    if (!graphicsTextWindow) {
        graphicsTextWindow = new GraphicsTextWindow(this);
        connect(this, &MainWindow::treeRowChanged, graphicsTextWindow, &GraphicsTextWindow::onTreeRowChanged);
    }
    // 定位到主窗口右侧
    QRect mainRect = this->frameGeometry();
    QPoint targetPos = mainRect.topRight();
    graphicsTextWindow->move(targetPos);
    graphicsTextWindow->resize(graphicsTextWindow->width(), mainRect.height());
    if (!graphicsTextWindow->isVisible()) {
        graphicsTextWindow->show();
    }
    graphicsTextWindow->raise();
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
            QByteArray bytes = cache->getResource(i);
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

// 更新播放按钮状态
void MainWindow::updatePlayActionState() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();

    // 默认不可用
    bool enable = false;

    if (index.isValid()) {
        // 计算深度
        int depth = 0;
        QModelIndex temp = index;
        while (temp.parent().isValid()) {
            temp = temp.parent();
            depth++;
        }

        // 判断条件
        if (depth == 1 && cache->getSignature(index.row()).startsWith("RIFF")) {
            enable = true;
        }
    }

    // 设置按钮是否可用
    playAudioAction->setEnabled(enable);
}

void MainWindow::treeSelectionChanged(const QModelIndex& current, const QModelIndex& previous) {
    updatePlayActionState();
    emit treeRowChanged(current);
}
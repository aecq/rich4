#include "gui/main_window.h"
#include "core/utils/check.h"
#include "core/io/parse.h"
#include "core/io/write.h"
#include "core/types/resource_header.h"
#include "core/types/spr_smp_header.h"
#include "core/utils/resource_model.h"
#include "core/utils/audio_player.h"
#include "gui/graphics_text_window.h"
#include <QApplication>
#include <QTreeView>
#include <QFileDialog>
#include <QMenuBar>
#include <QStatusBar>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QByteArray>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QProcess>
#include <QToolBar>
#include <QItemSelectionModel>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    resourceModel = new ResourceModel();
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

    saveCSVAction = fileMenu->addAction("Save CSV");
    saveCSVAction->setShortcut(QKeySequence::Save);
    connect(saveCSVAction, &QAction::triggered, this, &MainWindow::saveCSV);

    QAction* exportAction = fileMenu->addAction("Export Binary");
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportResource);

    QAction* replaceResource = fileMenu->addAction("Replace Resource...");
    connect(replaceResource, &QAction::triggered, this, &MainWindow::replaceResource);

    QMenu* showMenu = menuBar()->addMenu("&Show");

    playAudioAction = showMenu->addAction("Play Audio");
    playAudioAction->setShortcut(QKeySequence::Refresh);
    connect(playAudioAction, &QAction::triggered, this, &MainWindow::playAudio);
    playAudioAction->setEnabled(false);

    playFLCAction = showMenu->addAction("Play FLC");
    playFLCAction->setShortcut(QKeySequence::Refresh);
    connect(playFLCAction, &QAction::triggered, this, &MainWindow::playFLC);

    QAction* graphicsTextAction = showMenu->addAction("Graphics Text");
    connect(graphicsTextAction, &QAction::triggered, this, &MainWindow::openGraphicsTextWindow);
}

void MainWindow::createToolBar() {
    QToolBar* toolBar = addToolBar("Main");
    
    QAction* openAction = toolBar->addAction("Open");
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    toolBar->addSeparator();

    toolBar->addAction(playAudioAction);
    toolBar->addAction(playFLCAction);

    QAction* graphicsTextAction = toolBar->addAction("Graphics Text");
    connect(graphicsTextAction, &QAction::triggered, this, &MainWindow::openGraphicsTextWindow);
}

void MainWindow::setupConnections() {
    connect(this, &MainWindow::destroyed, qApp, &QApplication::quit);
    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::treeSelectionChanged);
    connect(treeModel, &QStandardItemModel::dataChanged, this, &MainWindow::onTreeDataChanged);
    connect(resourceModel, &ResourceModel::saved, this, [this](const QString& message) {
        statusBar()->showMessage(message, 2000);
    });
}

void MainWindow::openFile() {
    QString filepath = QFileDialog::getOpenFileName(this, "Open MKF File", "", "MKF Files (*.mkf)");
    if (filepath.isEmpty()) {
        return;
    }
    resourceModel->init(filepath);
    loadFileTree();
    statusBar()->showMessage(QString("Loaded %1 resource(s): %2").arg(resourceModel->n()).arg(filepath));
}

void MainWindow::playAudio() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }
    int depth = indexDepth(index);
    if (depth == 1 && resourceModel->getSignature(index.row()).startsWith("RIFF")) {
        statusBar()->showMessage("Playing audio at index:" + QString::number(index.row()));
        QByteArray data = resourceModel->getResource(index.row());
        AudioPlayer::instance().play(data);
    } else {
        statusBar()->showMessage("Please select an audio resource.");
    }
}

void MainWindow::playFLC() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }
    int depth = indexDepth(index);
    if (depth != 1) {
        return;
    }
    int row = index.row();
    QString defaultName = QString("%1%2.flc").arg(resourceModel->getBasename()).arg(row, 4, 10, QChar('0'));
    QString flcFolder = resourceModel->getCSVFolder() + "/flc/";
    QString filename = flcFolder + defaultName;
    QDir dir(flcFolder);
    dir.mkpath(flcFolder);
    // #region vlc
    // qDebug() << qgetenv("PATH");
    // 1. 检查文件路径是否为空
    QFile file(filename);
    if (!file.exists()) {
        resourceModel->exportBinary(row, filename);
        qDebug() << "Exported binary to:" << filename;
    }
    // 2. 创建QProcess（Qt跨平台进程调用类）
    QProcess *vlcProcess = new QProcess();
    // 设置进程结束后自动销毁，避免内存泄漏
    vlcProcess->setParent(nullptr);
    vlcProcess->setProgram("vlc"); // 直接调用PATH中的vlc命令
    // 3. 构建参数：播放文件 + 推荐参数（后台播放，不阻塞Qt程序）
    QStringList arguments;
    arguments << filename.replace("/", "\\")  // VLC 参数文件路径
            //   << "--play-and-exit"   // 播放完毕自动关闭VLC
              << "--one-instance";   // 单实例运行（避免重复打开VLC）
    vlcProcess->setArguments(arguments);
    // 4. 启动VLC进程（分离式启动，不阻塞主程序）
    bool startOk = vlcProcess->startDetached();
    if (startOk) {
        qDebug() << "VLC 启动成功，正在播放：" << filename;
    } else {
        qDebug() << "VLC 启动失败！错误信息：" << vlcProcess->errorString();
        qDebug() << "请确认 VLC 已添加到系统环境变量 PATH 中";
        statusBar()->showMessage("VLC 启动失败！错误信息：" + vlcProcess->errorString());
    }
    // 因为是startDetached，进程对象可以安全删除
    vlcProcess->deleteLater();
    // #endregion vlc
}

void MainWindow::saveCSV() {
    resourceModel->saveCSV();
}

void MainWindow::exportResource() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }
    int depth = indexDepth(index);
    if (depth != 1) {
        statusBar()->showMessage("Please select a resource to export.");
        return;
    }
    int row = index.row();
    QString defaultName = QString("%1%2%3.%4")
        .arg(resourceModel->getBasename())
        .arg(row, 4, 10, QChar('0'))
        .arg(legalFilename(resourceModel->getComment(row)))
        .arg(resourceModel->getType(row).size() > 0 ? resourceModel->getType(row) : "bin");
    QString filepath = QFileDialog::getSaveFileName(this, "Export Binary", defaultName, "Binary Files (*.*)");
    if (!filepath.isEmpty()) {
        resourceModel->exportBinary(row, filepath);
        statusBar()->showMessage(QString("Exported resource %1 to %2").arg(row).arg(filepath));
    }
}

void MainWindow::replaceResource() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();
    if (!index.isValid()) {
        return;
    }
    int depth = indexDepth(index);
    if (depth != 1) {
        statusBar()->showMessage("Please select a resource to replace.");
        return;
    }
    int row = index.row();
    try {
        // dialog choose resource binary file
        QString defaultName = QString("%1%2%3.%4")
            .arg(resourceModel->getBasename())
            .arg(row, 4, 10, QChar('0'))
            .arg(legalFilename(resourceModel->getComment(row)))
            .arg(resourceModel->getType(row).size() > 0 ? resourceModel->getType(row) : "bin");
        QString resourceFilepath = QFileDialog::getOpenFileName(this, "Resource Binary", defaultName, "Binary Files (*.*)");
        QString outFilepath = QFileDialog::getSaveFileName(this, "Save As", resourceModel->getBasename() + ".mkf", "MKF File (*.mkf)");
        if (!resourceFilepath.isEmpty() && !outFilepath.isEmpty()) {
            QFile inFile(resourceModel->getFilenamePrefix() + ".mkf");
            if (!inFile.open(QIODevice::ReadOnly)) {
                statusBar()->showMessage("Failed to open input file.");
                return;
            }
            QFile outFile(outFilepath);
            if (!outFile.open(QIODevice::WriteOnly)) {
                statusBar()->showMessage("Failed to open output file.");
                return;
            }
            QFile resourceFile(resourceFilepath);
            if (!resourceFile.open(QIODevice::ReadOnly)) {
                statusBar()->showMessage("Failed to open resource file.");
                return;
            }
            QByteArray resource = resourceFile.readAll();
            replaceBinary(inFile, outFile, resource, row);
        }
        statusBar()->showMessage(QString("Replaced resource %1. Saved %2").arg(row).arg(outFilepath));
    } catch (const std::exception& e) {
        statusBar()->showMessage(QString("Replace resource %1 failed: %2").arg(row).arg(e.what()));
    }
}

void MainWindow::openGraphicsTextWindow()
{
    if (!graphicsTextWindow) {
        graphicsTextWindow = new GraphicsTextWindow(this);
        connect(this, &MainWindow::treeRowChanged, graphicsTextWindow, &GraphicsTextWindow::onTreeRowChanged);
        connect(graphicsTextWindow, &GraphicsTextWindow::statusMessage, this, [this](const QString& message) {
            statusBar()->showMessage(message);
        });
        QModelIndex index = treeView->currentIndex();
        graphicsTextWindow->onTreeRowChanged(index);
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
        "#",
        "Info",
        "Type",
        "Comment",
    });

    // 根节点只在第 0 列表显示文件名，其他列留空
    QList<QStandardItem*> rootRow;
    rootRow << new QStandardItem(resourceModel->getBasename()/*ReadOnly*/)
            << new QStandardItem(""/*ReadOnly*/)
            << new QStandardItem(""/*ReadOnly*/)
            << new QStandardItem(""/*ReadOnly*/);
    treeModel->appendRow(rootRow);
    QStandardItem *rootItem = rootRow[0]; // 取根节点

    for (int i = 0; i < resourceModel->n(); i++) {
        // 从 resourceModel 取数据
        QString sig = resourceModel->getSignature(i);
        uint32_t uncompressed = resourceModel->getHeader(i).uncompressed_size;
        uint32_t compressed = resourceModel->getHeader(i).compressed_size;

        // 创建一行 4 个单元格
        QList<QStandardItem*> row;

        if (sig.startsWith("SPR") || sig.startsWith("SMP")) {
            QByteArray bytes = resourceModel->getResource(i);
            SPRSMPHeader header = parseSPRSMPHeader(bytes);
            // SPR || SMP Info: sig (num_chunks) uncsize ≥ csize
            row << new QStandardItem(QString::number(i)/*ReadOnly*/)
                << new QStandardItem(QString("%1 (%2) %3 ≥ %4")
                    .arg(sig).arg(QString::number(header.num_chunks))
                    .arg(QString::number(uncompressed)).arg(QString::number(compressed))/*ReadOnly*/)
                << new QStandardItem(resourceModel->getType(i)/*ReadOnly*/)
                << new QStandardItem(resourceModel->getComment(i));
            QStandardItem *rowItem = row[0];
            std::vector<GraphInfo> graphInfos = parseGraphInfos(bytes);
            for (int j = 0; j < graphInfos.size(); j++) {
                QList<QStandardItem*> chunkRow;
                // Chunk Info: w x h (x, y)
                chunkRow << new QStandardItem(QString::number(j)/*ReadOnly*/)
                         << new QStandardItem(QString("%1 x %2 (%3, %4)")
                            .arg(QString::number(graphInfos[j].width)).arg(QString::number(graphInfos[j].height))
                            .arg(QString::number(graphInfos[j].x)).arg(QString::number(graphInfos[j].y)/*ReadOnly*/))
                         << new QStandardItem(""/*ReadOnly*/)
                         << new QStandardItem(""/*ReadOnly*/);
                rowItem->appendRow(chunkRow);
            }
        } else {
            // Other Info: sig uncsize ≥ csize
            row << new QStandardItem(QString::number(i)/*ReadOnly*/)
                << new QStandardItem(QString("%1 %2 ≥ %3").arg(sig)
                    .arg(QString::number(uncompressed)).arg(QString::number(compressed))/*ReadOnly*/)
                << new QStandardItem(resourceModel->getType(i))
                << new QStandardItem(resourceModel->getComment(i));
        }
        rootItem->appendRow(row);
    }
    treeView->expandToDepth(0);
}

void MainWindow::onTreeDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight) {
    // 计算深度
    int depth = indexDepth(topLeft);
    // 判断条件
    if (depth != 1) {
        return;
    }
    int row = topLeft.row();
    int col = topLeft.column();
    if (col == 2) {
        resourceModel->setType(row, topLeft.data().toString());
    }
    if (col == 3) {
        resourceModel->setComment(row, topLeft.data().toString());
    }
}

// 更新播放按钮状态
void MainWindow::updatePlayActionState() {
    QItemSelectionModel* selectionModel = treeView->selectionModel();
    QModelIndex index = selectionModel->currentIndex();

    // 默认不可用
    bool enable = false;

    if (index.isValid()) {
        // 计算深度
        int depth = indexDepth(index);

        // 判断条件
        if (depth == 1 && resourceModel->getSignature(index.row()).startsWith("RIFF")) {
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
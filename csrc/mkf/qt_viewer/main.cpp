#include <QApplication>
#include <QMainWindow>
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
#include <QDataStream>
#include <QBitArray>
#include <QBuffer>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QToolBar>

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <functional>

// 第三方解压库函数声明
extern "C" {
    void mkf_decompress(void *dst, const void *src, size_t bufsz);
}

// ==================== 数据结构定义 ====================

#pragma pack(push, 1)
struct MKFHeader {
    uint32_t index_table_offset;
};

struct ResourceHeader {
    uint32_t uncompressed_size;
    uint32_t compressed_size;
    uint32_t image_data_offset;
    uint32_t image_data_size;
};

struct SPR_SMP_Header {
    char signature[4];
    uint32_t num_chunks;
    uint32_t start_offset;
};

struct GraphInfo {
    int16_t width;
    int16_t height;
    int16_t x;
    int16_t y;
    uint32_t gsize;
};
#pragma pack(pop)

// ==================== MKF文件解析器 ====================

class MKFParser {
public:
    struct ResourceInfo {
        uint32_t file_offset;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint32_t image_offset;
        uint32_t image_size;
        bool is_compressed;
        bool is_image_resource;
        
        // SPR/SMP特定字段
        bool is_spr_smp = false;
        char signature[4] = {0};
        uint32_t num_chunks = 0;
        uint32_t start_offset = 0;
        std::vector<GraphInfo> chunks;
    };

    MKFParser() = default;
    
    bool loadFile(const QString& filepath) {
        QFile file(filepath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        
        file_data_ = file.readAll();
        file.close();
        
        if (file_data_.size() < 4) {
            return false;
        }
        
        // 读取文件头
        MKFHeader header;
        memcpy(&header, file_data_.constData(), sizeof(MKFHeader));
        
        // 读取索引表
        if (header.index_table_offset >= static_cast<uint32_t>(file_data_.size())) {
            return false;
        }
        
        uint32_t index_table_size = file_data_.size() - header.index_table_offset;
        if (index_table_size % 4 != 0) {
            return false;
        }
        
        uint32_t num_resources = index_table_size / 4;
        resources_.clear();
        resources_.reserve(num_resources);
        
        // 读取每个资源的偏移量
        for (uint32_t i = 0; i < num_resources; ++i) {
            uint32_t offset;
            memcpy(&offset, file_data_.constData() + header.index_table_offset + i * 4, 4);
            
            if (offset >= static_cast<uint32_t>(file_data_.size())) {
                continue;
            }
            
            // 读取资源头
            ResourceInfo info;
            info.file_offset = offset;
            
            if (offset + sizeof(ResourceHeader) <= static_cast<uint32_t>(file_data_.size())) {
                ResourceHeader res_header;
                memcpy(&res_header, file_data_.constData() + offset, sizeof(ResourceHeader));
                
                info.uncompressed_size = res_header.uncompressed_size;
                info.compressed_size = res_header.compressed_size;
                info.image_offset = res_header.image_data_offset;
                info.image_size = res_header.image_data_size;
                info.is_compressed = (res_header.compressed_size != res_header.uncompressed_size);
                info.is_image_resource = (res_header.image_data_offset != 0 && res_header.image_data_size != 0);
            }
            
            resources_.push_back(info);
        }
        
        filepath_ = filepath;
        return true;
    }
    
    QByteArray getResourceData(uint32_t index, bool decompressed = true) {
        if (index >= resources_.size()) {
            return QByteArray();
        }
        
        const auto& res = resources_[index];
        
        // 读取压缩数据
        QByteArray compressed_data = file_data_.mid(res.file_offset + sizeof(ResourceHeader), 
                                                    res.compressed_size);
        
        if (!decompressed || !res.is_compressed) {
            return compressed_data;
        }
        
        // 解压数据
        QByteArray decompressed_data(res.uncompressed_size, 0);
        mkf_decompress(decompressed_data.data(), compressed_data.constData(), res.compressed_size);
        
        return decompressed_data;
    }
    
    bool parseSPRSMP(uint32_t index) {
        if (index >= resources_.size()) {
            return false;
        }
        
        auto& res = resources_[index];
        
        // 获取解压后的数据
        QByteArray data = getResourceData(index, true);
        if (data.size() < sizeof(SPR_SMP_Header)) {
            return false;
        }
        
        // 检查签名
        SPR_SMP_Header header;
        memcpy(&header, data.constData(), sizeof(SPR_SMP_Header));
        
        if (memcmp(header.signature, "SPR\0", 4) != 0 && 
            memcmp(header.signature, "SMP\0", 4) != 0) {
            return false;
        }
        
        res.is_spr_smp = true;
        memcpy(res.signature, header.signature, 4);
        res.num_chunks = header.num_chunks;
        res.start_offset = header.start_offset;
        res.chunks.clear();
        
        // 读取chunk表
        uint32_t chunk_table_offset = sizeof(SPR_SMP_Header);
        for (uint32_t i = 0; i < header.num_chunks; ++i) {
            if (chunk_table_offset + sizeof(GraphInfo) > static_cast<uint32_t>(data.size())) {
                break;
            }
            
            GraphInfo info;
            memcpy(&info, data.constData() + chunk_table_offset, sizeof(GraphInfo));
            res.chunks.push_back(info);
            chunk_table_offset += sizeof(GraphInfo);
        }
        
        return true;
    }
    
    QByteArray getImageData(uint32_t resource_index, uint32_t image_index) {
        if (resource_index >= resources_.size()) {
            return QByteArray();
        }
        
        auto& res = resources_[resource_index];
        if (!res.is_spr_smp || image_index >= res.chunks.size()) {
            return QByteArray();
        }
        
        // 获取解压后的资源数据
        QByteArray data = getResourceData(resource_index, true);
        
        // 计算图像数据偏移
        uint32_t image_offset = 0;
        if (memcmp(res.signature, "SPR\0", 4) == 0) {
            image_offset = res.start_offset + 512;
        } else {
            image_offset = res.start_offset;
        }
        
        // 累加前面图像的大小
        for (uint32_t i = 0; i < image_index; ++i) {
            image_offset += res.chunks[i].gsize;
        }
        
        const GraphInfo& info = res.chunks[image_index];
        if (image_offset + info.gsize > static_cast<uint32_t>(data.size())) {
            return QByteArray();
        }
        
        return data.mid(image_offset, info.gsize);
    }
    
    const std::vector<ResourceInfo>& getResources() const { return resources_; }
    QString getFilePath() const { return filepath_; }
    QByteArray getFileData() const { return file_data_; }
    
private:
    QString filepath_;
    QByteArray file_data_;
    std::vector<ResourceInfo> resources_;
};

// ==================== 自定义QHexEdit组件 ====================

class HexViewer : public QTextEdit {
public:
    HexViewer(QWidget* parent = nullptr) : QTextEdit(parent) {
        setReadOnly(true);
        setFont(QFont("Courier", 10));
    }
    
    void setData(const QByteArray& data, uint32_t offset = 0) {
        clear();
        
        if (data.isEmpty()) {
            setPlainText("No data available");
            return;
        }
        
        QString hex_dump;
        QString ascii_dump;
        
        for (int i = 0; i < data.size(); i += 16) {
            // 地址
            hex_dump += QString("%1: ").arg(offset + i, 8, 16, QChar('0')).toUpper();
            
            // 十六进制
            QString hex_part;
            QString ascii_part;
            
            for (int j = 0; j < 16; ++j) {
                if (i + j < data.size()) {
                    uint8_t byte = static_cast<uint8_t>(data[i + j]);
                    hex_part += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
                    
                    if (byte >= 32 && byte < 127) {
                        ascii_part += QChar(byte);
                    } else {
                        ascii_part += ".";
                    }
                } else {
                    hex_part += "   ";
                    ascii_part += " ";
                }
                
                if (j == 7) {
                    hex_part += " ";
                }
            }
            
            hex_dump += hex_part + " " + ascii_part + "\n";
        }
        
        setPlainText(hex_dump);
    }
};

// ==================== 图像预览器 ====================

class ImagePreviewer : public QWidget {
    Q_OBJECT
    
public:
    ImagePreviewer(QWidget* parent = nullptr) : QWidget(parent) {
        setBackgroundRole(QPalette::Dark);
        setAutoFillBackground(true);
        
        // 创建主布局
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        
        // 创建控制面板
        QWidget* controls = new QWidget(this);
        QHBoxLayout* controlLayout = new QHBoxLayout(controls);
        controlLayout->setContentsMargins(5, 5, 5, 5);
        
        QPushButton* zoomInBtn = new QPushButton("+", controls);
        QPushButton* zoomOutBtn = new QPushButton("-", controls);
        QPushButton* resetBtn = new QPushButton("Reset", controls);
        
        controlLayout->addWidget(zoomInBtn);
        controlLayout->addWidget(zoomOutBtn);
        controlLayout->addWidget(resetBtn);
        controlLayout->addStretch();
        
        // 创建滚动区域和图像标签
        scrollArea = new QScrollArea(this);
        scrollArea->setBackgroundRole(QPalette::Dark);
        imageLabel = new QLabel(scrollArea);
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setBackgroundRole(QPalette::Base);
        imageLabel->setAutoFillBackground(true);
        scrollArea->setWidget(imageLabel);
        scrollArea->setWidgetResizable(true);
        
        // 添加到主布局
        mainLayout->addWidget(controls);
        mainLayout->addWidget(scrollArea);
        
        // 连接信号槽
        connect(zoomInBtn, &QPushButton::clicked, this, &ImagePreviewer::zoomIn);
        connect(zoomOutBtn, &QPushButton::clicked, this, &ImagePreviewer::zoomOut);
        connect(resetBtn, &QPushButton::clicked, this, &ImagePreviewer::resetZoom);
        
        scaleFactor = 1.0;
    }
    
    void setImageData(const QByteArray& data, int width, int height, int bpp = 8) {
        if (data.isEmpty() || width <= 0 || height <= 0) {
            imageLabel->clear();
            return;
        }
        
        QImage img;
        
        if (bpp == 16) {
            // RGB565
            img = QImage(width, height, QImage::Format_RGB16);
            if (data.size() >= width * height * 2) {
                const uint16_t* pixels = reinterpret_cast<const uint16_t*>(data.constData());
                
                for (int y = 0; y < height; ++y) {
                    for (int x = 0; x < width; ++x) {
                        uint16_t pixel = pixels[y * width + x];
                        uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
                        uint8_t g = ((pixel >> 5) & 0x3F) * 255 / 63;
                        uint8_t b = (pixel & 0x1F) * 255 / 31;
                        img.setPixel(x, y, qRgb(r, g, b));
                    }
                }
            }
        } else {
            // 8-bit grayscale (临时方案)
            img = QImage(width, height, QImage::Format_Grayscale8);
            // 修复类型不匹配问题：将两个参数都转换为size_t
            size_t data_size = static_cast<size_t>(data.size());
            size_t required_size = static_cast<size_t>(width * height);
            size_t copy_size = std::min(data_size, required_size);
            if (copy_size > 0) {
                memcpy(img.bits(), data.constData(), copy_size);
            }
        }
        
        originalPixmap = QPixmap::fromImage(img);
        resetZoom();
    }
    
private slots:
    void zoomIn() {
        scaleFactor *= 1.25;
        updateImage();
    }
    
    void zoomOut() {
        scaleFactor *= 0.8;
        updateImage();
    }
    
    void resetZoom() {
        scaleFactor = 1.0;
        updateImage();
    }
    
private:
    void updateImage() {
        if (originalPixmap.isNull()) {
            imageLabel->clear();
            return;
        }
        
        QSize size = originalPixmap.size() * scaleFactor;
        imageLabel->setPixmap(originalPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    QScrollArea* scrollArea;
    QLabel* imageLabel;
    QPixmap originalPixmap;
    double scaleFactor;
};

// ==================== 主窗口 ====================

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent), parser(new MKFParser()) {
        setupUI();
        setupConnections();
        setWindowTitle("MKF File Viewer");
        resize(1200, 800);
    }
    
    ~MainWindow() {
        delete parser;
    }
    
private slots:
    void openFile() {
        QString filepath = QFileDialog::getOpenFileName(this, "Open MKF File", "", "MKF Files (*.mkf)");
        if (filepath.isEmpty()) {
            return;
        }
        
        if (!parser->loadFile(filepath)) {
            QMessageBox::critical(this, "Error", "Failed to load MKF file");
            return;
        }
        
        loadFileTree();
        statusBar()->showMessage(QString("Loaded: %1").arg(filepath));
    }
    
    void extractSelected() {
        QModelIndex index = treeView->selectionModel()->currentIndex();
        if (!index.isValid()) {
            return;
        }
        
        QString filepath = QFileDialog::getSaveFileName(this, "Extract File");
        if (filepath.isEmpty()) {
            return;
        }
        
        QByteArray data = getDataForIndex(index);
        if (data.isEmpty()) {
            QMessageBox::warning(this, "Warning", "No data to extract");
            return;
        }
        
        QFile file(filepath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            statusBar()->showMessage(QString("Extracted to: %1").arg(filepath));
        }
    }
    
    void findNextSPRSMP() {
        int startIndex = 0;
        QModelIndex current = treeView->selectionModel()->currentIndex();
        
        if (current.isValid()) {
            // 如果是资源节点，获取资源索引
            QVariant resourceIndex = treeModel->data(current, Qt::UserRole + 1);
            if (resourceIndex.isValid()) {
                startIndex = resourceIndex.toInt() + 1;
            }
        }
        
        for (int i = startIndex; i < static_cast<int>(parser->getResources().size()); ++i) {
            // 快速检查前4字节
            QByteArray compressed = parser->getResourceData(i, false);
            if (compressed.size() >= 4) {
                char signature[4];
                mkf_decompress(signature, compressed.constData(), 4);
                
                if (memcmp(signature, "SPR\0", 4) == 0 || 
                    memcmp(signature, "SMP\0", 4) == 0) {
                    // 展开并选中该资源
                    selectResource(i);
                    statusBar()->showMessage(QString("Found SPR/SMP resource at index %1").arg(i));
                    return;
                }
            }
        }
        
        statusBar()->showMessage("No more SPR/SMP resources found");
    }
    
    void treeSelectionChanged(const QModelIndex& current, const QModelIndex& previous) {
        Q_UNUSED(previous);
        
        updatePropertyTable(current);
        updateHexViewer(current);
        updateImagePreview(current);
    }
    
private:
    void setupUI() {
        // 创建主分割器
        QSplitter* mainSplitter = new QSplitter(Qt::Horizontal, this);
        
        // 左侧：树状视图
        QWidget* leftPanel = new QWidget(mainSplitter);
        QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
        
        treeView = new QTreeView(leftPanel);
        treeModel = new QStandardItemModel(this);
        treeModel->setHorizontalHeaderLabels({"MKF Structure"});
        treeView->setModel(treeModel);
        treeView->setHeaderHidden(true);
        
        leftLayout->addWidget(treeView);
        
        // 右侧：属性面板、十六进制查看器、图像预览器
        QSplitter* rightSplitter = new QSplitter(Qt::Vertical, mainSplitter);
        
        // 属性表格
        propertyTable = new QTableView(rightSplitter);
        propertyModel = new QStandardItemModel(this);
        propertyModel->setHorizontalHeaderLabels({"Property", "Value"});
        propertyTable->setModel(propertyModel);
        propertyTable->horizontalHeader()->setStretchLastSection(true);
        
        // 十六进制查看器
        hexViewer = new HexViewer(rightSplitter);
        
        // 图像预览器
        imagePreviewer = new ImagePreviewer(rightSplitter);
        
        rightSplitter->addWidget(propertyTable);
        rightSplitter->addWidget(hexViewer);
        rightSplitter->addWidget(imagePreviewer);
        rightSplitter->setSizes({200, 300, 300});
        
        mainSplitter->addWidget(leftPanel);
        mainSplitter->addWidget(rightSplitter);
        mainSplitter->setSizes({300, 900});
        
        setCentralWidget(mainSplitter);
        
        // 创建菜单栏
        createMenuBar();
        
        // 创建工具栏
        createToolBar();
        
        // 状态栏
        statusBar()->showMessage("Ready");
    }
    
    void createMenuBar() {
        QMenu* fileMenu = menuBar()->addMenu("&File");
        // 修复Qt6弃用的API调用
        QAction* openAction = fileMenu->addAction("&Open MKF...");
        openAction->setShortcut(QKeySequence::Open);
        connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
        
        QAction* extractAction = fileMenu->addAction("&Extract Selected...");
        connect(extractAction, &QAction::triggered, this, &MainWindow::extractSelected);
        
        fileMenu->addSeparator();
        
        QAction* exitAction = fileMenu->addAction("E&xit");
        exitAction->setShortcut(QKeySequence::Quit);
        connect(exitAction, &QAction::triggered, this, &QWidget::close);
        
        QMenu* editMenu = menuBar()->addMenu("&Edit");
        QAction* findAction = editMenu->addAction("&Find Next SPR/SMP");
        findAction->setShortcut(QKeySequence::FindNext);
        connect(findAction, &QAction::triggered, this, &MainWindow::findNextSPRSMP);
    }
    
    void createToolBar() {
        QToolBar* toolBar = addToolBar("Main");
        
        QAction* openAction = toolBar->addAction("Open");
        connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
        
        QAction* extractAction = toolBar->addAction("Extract");
        connect(extractAction, &QAction::triggered, this, &MainWindow::extractSelected);
        
        QAction* findAction = toolBar->addAction("Find SPR/SMP");
        connect(findAction, &QAction::triggered, this, &MainWindow::findNextSPRSMP);
    }
    
    void setupConnections() {
        connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
                this, &MainWindow::treeSelectionChanged);
    }
    
    void loadFileTree() {
        treeModel->clear();
        treeModel->setHorizontalHeaderLabels({"MKF Structure"});
        
        QStandardItem* rootItem = new QStandardItem(QFileInfo(parser->getFilePath()).fileName());
        rootItem->setData(-1, Qt::UserRole + 1); // 文件节点
        treeModel->appendRow(rootItem);
        
        const auto& resources = parser->getResources();
        for (size_t i = 0; i < resources.size(); ++i) {
            const auto& res = resources[i];
            QStandardItem* resItem = new QStandardItem(QString("Resource %1").arg(i));
            resItem->setData(static_cast<int>(i), Qt::UserRole + 1);
            resItem->setData("resource", Qt::UserRole + 2);
            
            // 添加压缩数据节点
            QStandardItem* compressedItem = new QStandardItem("Compressed Data");
            compressedItem->setData(static_cast<int>(i), Qt::UserRole + 1);
            compressedItem->setData("compressed", Qt::UserRole + 2);
            
            // 添加解压数据节点
            QStandardItem* decompressedItem = new QStandardItem("Decompressed Data");
            decompressedItem->setData(static_cast<int>(i), Qt::UserRole + 1);
            decompressedItem->setData("decompressed", Qt::UserRole + 2);
            
            // 检查是否为SPR/SMP
            if (parser->parseSPRSMP(i)) {
                decompressedItem->setText("Decompressed Data (SPR/SMP)");
                
                const auto& resInfo = resources[i];
                for (size_t j = 0; j < resInfo.chunks.size(); ++j) {
                    const auto& chunk = resInfo.chunks[j];
                    QStandardItem* imgItem = new QStandardItem(
                        QString("Image %1 (%2x%3)").arg(j).arg(chunk.width).arg(chunk.height));
                    imgItem->setData(static_cast<int>(i), Qt::UserRole + 1);
                    imgItem->setData(static_cast<int>(j), Qt::UserRole + 3);
                    imgItem->setData("image", Qt::UserRole + 2);
                    decompressedItem->appendRow(imgItem);
                }
            }
            
            if (res.is_compressed) {
                resItem->appendRow(compressedItem);
                resItem->appendRow(decompressedItem);
            } else {
                resItem->appendRow(decompressedItem);
            }
            
            rootItem->appendRow(resItem);
        }
        
        treeView->expand(rootItem->index());
    }
    
    void selectResource(int index) {
        QModelIndex root = treeModel->index(0, 0);
        if (root.isValid()) {
            QModelIndex resIndex = treeModel->index(index, 0, root);
            if (resIndex.isValid()) {
                treeView->setCurrentIndex(resIndex);
                treeView->expand(resIndex);
            }
        }
    }
    
    QByteArray getDataForIndex(const QModelIndex& index) {
        QVariant type = treeModel->data(index, Qt::UserRole + 2);
        QVariant resourceIndex = treeModel->data(index, Qt::UserRole + 1);
        QVariant imageIndex = treeModel->data(index, Qt::UserRole + 3);
        
        if (!type.isValid() || !resourceIndex.isValid()) {
            return QByteArray();
        }
        
        QString nodeType = type.toString();
        int resIdx = resourceIndex.toInt();
        
        if (nodeType == "resource") {
            // 返回整个资源数据（压缩的）
            return parser->getResourceData(resIdx, false);
        } else if (nodeType == "compressed") {
            return parser->getResourceData(resIdx, false);
        } else if (nodeType == "decompressed") {
            return parser->getResourceData(resIdx, true);
        } else if (nodeType == "image") {
            if (imageIndex.isValid()) {
                return parser->getImageData(resIdx, imageIndex.toInt());
            }
        }
        
        return QByteArray();
    }
    
    void updatePropertyTable(const QModelIndex& index) {
        propertyModel->removeRows(0, propertyModel->rowCount());
        
        if (!index.isValid()) {
            return;
        }
        
        QVariant type = treeModel->data(index, Qt::UserRole + 2);
        QVariant resourceIndex = treeModel->data(index, Qt::UserRole + 1);
        QVariant imageIndex = treeModel->data(index, Qt::UserRole + 3);
        
        if (!type.isValid()) {
            return;
        }
        
        QString nodeType = type.toString();
        
        if (nodeType == "resource") {
            int resIdx = resourceIndex.toInt();
            const auto& resources = parser->getResources();
            if (resIdx >= 0 && resIdx < static_cast<int>(resources.size())) {
                const auto& res = resources[resIdx];
                
                addProperty("Type", "Resource");
                addProperty("Resource Index", QString::number(resIdx));
                addProperty("File Offset", QString("0x%1").arg(res.file_offset, 8, 16, QChar('0')));
                addProperty("Compressed Size", QString("%1 bytes").arg(res.compressed_size));
                addProperty("Uncompressed Size", QString("%1 bytes").arg(res.uncompressed_size));
                addProperty("Is Compressed", res.is_compressed ? "Yes" : "No");
                addProperty("Is Image Resource", res.is_image_resource ? "Yes" : "No");
                
                if (res.is_spr_smp) {
                    addProperty("SPR/SMP Signature", QString(res.signature));
                    addProperty("Number of Images", QString::number(res.num_chunks));
                }
            }
        } else if (nodeType == "image") {
            int resIdx = resourceIndex.toInt();
            int imgIdx = imageIndex.toInt();
            const auto& resources = parser->getResources();
            
            if (resIdx >= 0 && resIdx < static_cast<int>(resources.size())) {
                const auto& res = resources[resIdx];
                if (res.is_spr_smp && imgIdx >= 0 && imgIdx < static_cast<int>(res.chunks.size())) {
                    const auto& chunk = res.chunks[imgIdx];
                    
                    addProperty("Type", "Image");
                    addProperty("Resource Index", QString::number(resIdx));
                    addProperty("Image Index", QString::number(imgIdx));
                    addProperty("Width", QString::number(chunk.width));
                    addProperty("Height", QString::number(chunk.height));
                    addProperty("X Position", QString::number(chunk.x));
                    addProperty("Y Position", QString::number(chunk.y));
                    addProperty("Data Size", QString("%1 bytes").arg(chunk.gsize));
                    
                    // 判断位深（简单启发式）
                    int pixelCount = chunk.width * chunk.height;
                    if (pixelCount > 0) {
                        int bpp = (chunk.gsize * 8) / pixelCount;
                        addProperty("Bits per Pixel", QString::number(bpp));
                    }
                }
            }
        }
    }
    
    void addProperty(const QString& name, const QString& value) {
        QList<QStandardItem*> items;
        items.append(new QStandardItem(name));
        items.append(new QStandardItem(value));
        propertyModel->appendRow(items);
    }
    
    void updateHexViewer(const QModelIndex& index) {
        QByteArray data = getDataForIndex(index);
        if (!data.isEmpty()) {
            hexViewer->setData(data);
        } else {
            hexViewer->setPlainText("No data available");
        }
    }
    
    void updateImagePreview(const QModelIndex& index) {
        QVariant type = treeModel->data(index, Qt::UserRole + 2);
        QVariant resourceIndex = treeModel->data(index, Qt::UserRole + 1);
        QVariant imageIndex = treeModel->data(index, Qt::UserRole + 3);
        
        if (type.toString() == "image" && resourceIndex.isValid() && imageIndex.isValid()) {
            int resIdx = resourceIndex.toInt();
            int imgIdx = imageIndex.toInt();
            const auto& resources = parser->getResources();
            
            if (resIdx >= 0 && resIdx < static_cast<int>(resources.size())) {
                const auto& res = resources[resIdx];
                if (res.is_spr_smp && imgIdx >= 0 && imgIdx < static_cast<int>(res.chunks.size())) {
                    const auto& chunk = res.chunks[imgIdx];
                    QByteArray imageData = parser->getImageData(resIdx, imgIdx);
                    
                    // 判断位深
                    int pixelCount = chunk.width * chunk.height;
                    int bpp = 8;
                    if (pixelCount > 0) {
                        bpp = (chunk.gsize * 8) / pixelCount;
                    }
                    
                    imagePreviewer->setImageData(imageData, chunk.width, chunk.height, bpp);
                    return;
                }
            }
        }
        
        // 清空图像预览
        imagePreviewer->setImageData(QByteArray(), 0, 0);
    }
    
private:
    MKFParser* parser;
    
    // UI组件
    QTreeView* treeView;
    QStandardItemModel* treeModel;
    
    QTableView* propertyTable;
    QStandardItemModel* propertyModel;
    
    HexViewer* hexViewer;
    ImagePreviewer* imagePreviewer;
};

// ==================== 主函数 ====================

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // 设置应用程序样式
    app.setStyle("Fusion");
    
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

#include "main.moc"
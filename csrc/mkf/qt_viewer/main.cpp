#include <QHeaderView>
#include <QPushButton>
#include <QGroupBox>
#include <QTabWidget>
#include <QWheelEvent>
#include <QApplication>
#include <QMainWindow>
#include <QTreeView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QScrollArea>
#include <QFormLayout>
#include <QAbstractItemModel>
#include <QFile>
#include <QDataStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QtEndian>
#include <QVector>
#include <QDebug>

#include "mkf_structs.h"

// ==========================================
// 1. 数据模型项 (TreeItem)
// ==========================================

enum ItemType {
    RootType,
    ResourceType,
    CompressedDataType,
    DecompressedDataType,
    ImageChunkType
};

class TreeItem {
public:
    explicit TreeItem(const QVector<QVariant> &data, ItemType type, TreeItem *parent = nullptr)
        : m_itemData(data), m_type(type), m_parentItem(parent) {}

    ~TreeItem() { qDeleteAll(m_childItems); }

    void appendChild(TreeItem *child) { m_childItems.append(child); }
    TreeItem *child(int row) { return m_childItems.value(row); }
    int childCount() const { return m_childItems.count(); }
    int columnCount() const { return m_itemData.count(); }
    QVariant data(int column) const { return m_itemData.value(column); }
    int row() const {
        if (m_parentItem) return m_parentItem->m_childItems.indexOf(const_cast<TreeItem*>(this));
        return 0;
    }
    TreeItem *parentItem() { return m_parentItem; }

    // 存储节点特有的元数据
    ItemType m_type;
    qint64 fileOffset = 0;      // 在文件中的绝对偏移
    qint64 size = 0;            // 当前数据块大小
    qint64 decompressedSize = 0;// 解压后大小
    
    // SPR/SMP 图像属性
    int imgWidth = 0;
    int imgHeight = 0;
    int imgX = 0;
    int imgY = 0;
    
    // 缓存数据 (用于显示或进一步解析)
    QByteArray cachedData; 
    bool isParsed = false;      // 标记是否已经解析过子节点（懒加载用）
    
    // 辅助字段
    int resourceId = -1;
    bool isCompressed = false;

private:
    QVector<TreeItem*> m_childItems;
    QVector<QVariant> m_itemData;
    TreeItem *m_parentItem;
};

// ==========================================
// 2. 数据模型 (MkfModel)
// ==========================================

class MkfModel : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit MkfModel(const QString &filePath, QObject *parent = nullptr);
    ~MkfModel();

    QVariant data(const QModelIndex &index, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    TreeItem* getItem(const QModelIndex &index) const;
    void loadFile();
    
    // 供View调用，用于懒加载解析 SPR/SMP
    void parseDecompressedData(const QModelIndex &index);

    // 获取当前文件的句柄
    QFile* getFileHandle() { return &m_file; }

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

private:
    void setupModelData(TreeItem *parent);
    
    TreeItem *rootItem;
    QFile m_file;
    QString m_filePath;
};

MkfModel::MkfModel(const QString &filePath, QObject *parent)
    : QAbstractItemModel(parent), m_filePath(filePath) {
    rootItem = new TreeItem({tr("Name"), tr("Size")}, RootType);
    loadFile();
}

MkfModel::~MkfModel() {
    delete rootItem;
    if (m_file.isOpen()) m_file.close();
}

int MkfModel::columnCount(const QModelIndex &parent) const {
    return rootItem->columnCount();
}

QVariant MkfModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return QVariant();
    if (role != Qt::DisplayRole) return QVariant();

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    return item->data(index.column());
}

Qt::ItemFlags MkfModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return QAbstractItemModel::flags(index);
}

QVariant MkfModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);
    return QVariant();
}

QModelIndex MkfModel::index(int row, int column, const QModelIndex &parent) const {
    if (!hasIndex(row, column, parent)) return QModelIndex();

    TreeItem *parentItem;
    if (!parent.isValid()) parentItem = rootItem;
    else parentItem = static_cast<TreeItem*>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem) return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex MkfModel::parent(const QModelIndex &index) const {
    if (!index.isValid()) return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem*>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem) return QModelIndex();
    return createIndex(parentItem->row(), 0, parentItem);
}

int MkfModel::rowCount(const QModelIndex &parent) const {
    if (parent.column() > 0) return 0;

    TreeItem *parentItem;
    if (!parent.isValid()) {
      parentItem = rootItem;
    } else {
      parentItem = static_cast<TreeItem*>(parent.internalPointer());
    }

    return parentItem->childCount();
}

bool MkfModel::hasChildren(const QModelIndex &parent) const {
    if (!parent.isValid()) return true; // 根节点总是有资源
    
    TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
    
    // 如果是解压数据节点且尚未解析，我们“假定”它有子节点（为了显示箭头）
    if (item->m_type == DecompressedDataType && !item->isParsed) {
        return true; 
    }
    return item->childCount() > 0;
}

// 3. 实现 canFetchMore：告诉视图这个节点是否可以进一步获取数据
bool MkfModel::canFetchMore(const QModelIndex &parent) const {
    if (!parent.isValid()) return false;
    TreeItem *item = static_cast<TreeItem*>(parent.internalPointer());
    
    // 只有未解析的解压数据节点可以 fetchMore
    return (item->m_type == DecompressedDataType && !item->isParsed);
}

// 4. 实现 fetchMore：当用户点击展开箭头时，系统会自动调用此函数
void MkfModel::fetchMore(const QModelIndex &parent) {
    if (!parent.isValid()) return;
    
    // 在这里安全地触发解析逻辑
    parseDecompressedData(parent);
}

TreeItem* MkfModel::getItem(const QModelIndex &index) const {
    if (index.isValid()) {
        TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
        if (item) return item;
    }
    return rootItem;
}

void MkfModel::loadFile() {
    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::ReadOnly)) return;

    QDataStream in(&m_file);
    in.setByteOrder(QDataStream::LittleEndian);

    // 1. 读取 index_table_offset
    quint32 indexTableOffset;
    in >> indexTableOffset;

    qint64 fileSize = m_file.size();
    if (indexTableOffset < 4 || indexTableOffset >= fileSize) return;

    // 2. 读取索引表
    m_file.seek(indexTableOffset);
    QVector<quint32> offsets;
    quint32 tempOffset;
    while (!in.atEnd()) {
        in >> tempOffset;
        offsets.append(tempOffset);
    }

    int resourceCount = offsets.size();

    // 3. 构建资源节点
    for (int i = 0; i < resourceCount; ++i) {
        quint32 currentOffset = offsets[i];
        m_file.seek(currentOffset);

        ResourceHeader header;
        in.readRawData((char*)&header, sizeof(ResourceHeader));
        
        // 处理字节序
        header.uncompressed_size = qFromLittleEndian(header.uncompressed_size);
        header.compressed_size = qFromLittleEndian(header.compressed_size);
        header.image_data_offset = qFromLittleEndian(header.image_data_offset);
        header.image_data_size = qFromLittleEndian(header.image_data_size);

        TreeItem *resItem = new TreeItem({QString("Resource %1").arg(i), QString::number(header.compressed_size)}, ResourceType, rootItem);
        resItem->resourceId = i;
        resItem->fileOffset = currentOffset; // 资源起始（含头）
        resItem->size = 16 + header.compressed_size;
        resItem->decompressedSize = header.uncompressed_size;
        rootItem->appendChild(resItem);

        // 判断是否压缩
        bool isCompressed = (header.compressed_size != header.uncompressed_size);
        
        // 数据偏移 = 资源起始 + 16字节头
        qint64 dataOffset = currentOffset + 16;

        if (isCompressed) {
            // 节点：Compressed Data
            TreeItem *compItem = new TreeItem({"Compressed Data", QString::number(header.compressed_size)}, CompressedDataType, resItem);
            compItem->fileOffset = dataOffset;
            compItem->size = header.compressed_size;
            compItem->decompressedSize = header.uncompressed_size; // 用于解压提示
            resItem->appendChild(compItem);

            // 节点：Decompressed Data (占位，需要解压后才有数据)
            TreeItem *decompItem = new TreeItem({"Decompressed Data", QString::number(header.uncompressed_size)}, DecompressedDataType, compItem);
            decompItem->size = header.uncompressed_size;
            decompItem->isCompressed = true; // 标记来源是压缩的
            compItem->appendChild(decompItem);
        } else {
            // 节点：Decompressed Data (直接存储在文件中)
            TreeItem *decompItem = new TreeItem({"Decompressed Data", QString::number(header.uncompressed_size)}, DecompressedDataType, resItem);
            decompItem->fileOffset = dataOffset;
            decompItem->size = header.uncompressed_size;
            decompItem->isCompressed = false;
            resItem->appendChild(decompItem);
        }
    }
}

void MkfModel::parseDecompressedData(const QModelIndex &index) {
    TreeItem *item = getItem(index);
    if (!item || item->m_type != DecompressedDataType || item->isParsed) return;
    
    // 如果数据为空，说明还没读取或解压
    if (item->cachedData.isEmpty()) {
        // 尝试获取数据
        if (item->isCompressed) {
            // 需要从父节点(Compressed Data)获取原始数据并解压
            TreeItem *parent = item->parentItem();
            m_file.seek(parent->fileOffset);
            QByteArray src = m_file.read(parent->size);
            
            QByteArray dst;
            dst.resize(item->size);
            
            // 调用外部C函数
            mkf_decompress(dst.data(), src.constData(), dst.size());
            item->cachedData = dst;
        } else {
            // 直接读取
            m_file.seek(item->fileOffset);
            item->cachedData = m_file.read(item->size);
        }
    }

    if (item->cachedData.size() < 12) {
        item->isParsed = true;
        return; 
    }

    // 检查 SPR/SMP 签名
    const char* dataPtr = item->cachedData.constData();
    GraphBundleHeader header;
    memcpy(&header, dataPtr, sizeof(GraphBundleHeader));
    
    // 字节序处理 (width, offset 等)
    header.num_chunks = qFromLittleEndian(header.num_chunks);
    header.start_offset = qFromLittleEndian(header.start_offset);

    bool isSPR = (strncmp(header.signature, "SPR", 3) == 0);
    bool isSMP = (strncmp(header.signature, "SMP", 3) == 0);

    if (isSPR || isSMP) {
        // 开始解析 Chunks
        beginInsertRows(index, 0, header.num_chunks - 1);
        
        qint64 currentGraphOffset = 0;
        if (isSPR) currentGraphOffset = header.start_offset + 512;
        else currentGraphOffset = header.start_offset;

        // 遍历 Chunk Table
        // Chunk table starts at offset 12
        const char* tablePtr = dataPtr + 12;

        for (uint32_t i = 0; i < header.num_chunks; ++i) {
            GraphInfo info;
            memcpy(&info, tablePtr + i * sizeof(GraphInfo), sizeof(GraphInfo));

            info.width = qFromLittleEndian(info.width);
            info.height = qFromLittleEndian(info.height);
            info.x = qFromLittleEndian(info.x);
            info.y = qFromLittleEndian(info.y);
            info.gsize = qFromLittleEndian(info.gsize);

            TreeItem *chunkItem = new TreeItem({QString("Image %1").arg(i), QString::number(info.gsize)}, ImageChunkType, item);
            chunkItem->imgWidth = info.width;
            chunkItem->imgHeight = info.height;
            chunkItem->imgX = info.x;
            chunkItem->imgY = info.y;
            chunkItem->size = info.gsize;
            
            // 提取该 Chunk 的数据到缓存
            if (currentGraphOffset + info.gsize <= item->cachedData.size()) {
                 chunkItem->cachedData = item->cachedData.mid(currentGraphOffset, info.gsize);
            }
            
            item->appendChild(chunkItem);
            
            currentGraphOffset += info.gsize;
        }
        endInsertRows();
    }

    item->isParsed = true;
}

// ==========================================
// 3. 主窗口 (MainWindow)
// ==========================================

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void onSelectionChanged(const QModelIndex &current, const QModelIndex &previous);
    void extractItem();

private:
    void updateProperties(TreeItem *item);
    void updateHexView(const QByteArray &data);
    void updateImageView(TreeItem *item);
    void refreshImagePreview();
    bool eventFilter(QObject *obj, QEvent *event);

    QTreeView *treeView;
    MkfModel *model = nullptr;
    
    // 属性控件
    QLabel *lblOffset;
    QLabel *lblSize;
    QLabel *lblDecompSize;
    QLabel *lblImgInfo; // W, H, X, Y
    QLabel *lblBitDepth;
    QLabel *lblZoom;
    
    // 查看器
    QTextEdit *hexView;
    QLabel *imageLabel;
    QScrollArea *imageScroll;
    
    QPushButton *btnExtract;

    double m_scaleFactor = 1.0;
    QImage m_sourceImage; // 保存原始未缩放的图像
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("MKF File Viewer");
    resize(1000, 700);

    QWidget *centralWidget = new QWidget;
    setCentralWidget(centralWidget);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter);

    // 左侧：树状视图
    QWidget *leftContainer = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftContainer);
    treeView = new QTreeView;
    treeView->setAlternatingRowColors(true);
    leftLayout->addWidget(treeView);
    
    QPushButton *btnOpen = new QPushButton("Open MKF File");
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openFile);
    leftLayout->addWidget(btnOpen);

    splitter->addWidget(leftContainer);

    // 右侧：属性与内容
    QWidget *rightContainer = new QWidget;
    QVBoxLayout *rightLayout = new QVBoxLayout(rightContainer);
    
    // 属性组
    QGroupBox *propGroup = new QGroupBox("Properties");
    QFormLayout *formLayout = new QFormLayout;
    lblOffset = new QLabel("-");
    lblSize = new QLabel("-");
    lblDecompSize = new QLabel("-");
    lblImgInfo = new QLabel("-");
    lblBitDepth = new QLabel("-");
    lblZoom = new QLabel("100%");
    
    formLayout->addRow("File Offset:", lblOffset);
    formLayout->addRow("Size (Bytes):", lblSize);
    formLayout->addRow("Decompressed Size:", lblDecompSize);
    formLayout->addRow("Image Info:", lblImgInfo);
    formLayout->addRow("Bit Depth:", lblBitDepth);
    formLayout->addRow("Zoom Level:", lblZoom);

    propGroup->setLayout(formLayout);
    rightLayout->addWidget(propGroup);

    // 导出按钮
    btnExtract = new QPushButton("Extract Selected Object...");
    btnExtract->setEnabled(false);
    connect(btnExtract, &QPushButton::clicked, this, &MainWindow::extractItem);
    rightLayout->addWidget(btnExtract);

    // 内容查看区 (Tab: Hex | Image)
    QTabWidget *tabWidget = new QTabWidget;
    
    // Hex View
    hexView = new QTextEdit;
    hexView->setReadOnly(true);
    hexView->setFont(QFont("Courier New", 10));
    tabWidget->addTab(hexView, "Hex View");

    // Image View 设置
    imageScroll = new QScrollArea;
    imageLabel = new QLabel;
    imageLabel->setAlignment(Qt::AlignCenter); // 图片居中
    imageLabel->setScaledContents(false);      // 禁止自动拉伸，由我们手动控制
    
    // --- 关键：安装事件过滤器 ---
    // 我们同时监听 scrollArea 和 label，以确保鼠标在哪里都能缩放
    imageLabel->installEventFilter(this);
    imageScroll->installEventFilter(this);

    imageScroll->setWidget(imageLabel);
    imageScroll->setWidgetResizable(true); // 允许 ScrollArea 调整 Widget 大小
    tabWidget->addTab(imageScroll, "Image Preview");

    rightLayout->addWidget(tabWidget);
    splitter->addWidget(rightContainer);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
}

MainWindow::~MainWindow() {}

void MainWindow::openFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Open MKF File", "", "MKF Files (*.mkf);;All Files (*)");
    if (fileName.isEmpty()) return;

    if (model) delete model;
    model = new MkfModel(fileName, this);
    treeView->setModel(model);

    // --- 修复列宽逻辑 ---
    QHeaderView *header = treeView->header();
    // 第一列 (Name) 自动拉伸填充剩余空间
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    // 第二列 (Size) 根据内容调整宽度
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    // ------------------

    connect(treeView->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onSelectionChanged);

    // 展开第一层
    treeView->expandToDepth(0);
}

void MainWindow::onSelectionChanged(const QModelIndex &current, const QModelIndex &previous) {
    if (!current.isValid()) return;

    TreeItem *item = model->getItem(current);
    updateProperties(item);
    btnExtract->setEnabled(true);

    // 获取数据用于显示
    QByteArray dataToShow;

    // 准备数据
    if (!item->cachedData.isEmpty()) {
        dataToShow = item->cachedData;
    } else if (item->size > 0 && item->fileOffset > 0) {
        // 从文件读取原始数据
        QFile *f = model->getFileHandle();
        if (f->isOpen()) {
            f->seek(item->fileOffset);
            dataToShow = f->read(qMin(item->size, (qint64)1024 * 1024)); // 限制读取大小以防界面卡死
        }
    }

    updateHexView(dataToShow);
    updateImageView(item);
}

void MainWindow::updateProperties(TreeItem *item) {
    lblOffset->setText(QString::number(item->fileOffset));
    lblSize->setText(QString::number(item->size));
    lblDecompSize->setText(item->decompressedSize > 0 ? QString::number(item->decompressedSize) : "-");
    
    if (item->m_type == ImageChunkType) {
        lblImgInfo->setText(QString("Width:%1 Height:%2 X:%3 Y:%4")
                            .arg(item->imgWidth).arg(item->imgHeight)
                            .arg(item->imgX).arg(item->imgY));
	// --- 新增：判断位深逻辑 ---
        qint64 pixelCount = (qint64)item->imgWidth * item->imgHeight;
        
        if (pixelCount > 0) {
            if (item->size >= pixelCount * 2) {
                lblBitDepth->setText("16-bit (RGB565)");
            } else if (item->size >= pixelCount) {
                lblBitDepth->setText("8-bit (Indexed)");
            } else {
                lblBitDepth->setText("Unknown (Size Mismatch)");
            }
        } else {
            lblBitDepth->setText("Invalid Dimensions");
        }
        // ------------------------
    } else {
        lblImgInfo->setText("-");
        lblBitDepth->setText("-");
    }
}

void MainWindow::updateHexView(const QByteArray &data) {
    // 简单的 Hex dump
    QString hexStr;
    int len = qMin(data.size(), 2048); // 只显示前2KB
    const unsigned char *ptr = (const unsigned char*)data.constData();
    
    for (int i = 0; i < len; ++i) {
        if (i % 16 == 0) hexStr += QString("%1: ").arg(i, 4, 16, QChar('0'));
        hexStr += QString("%1 ").arg(ptr[i], 2, 16, QChar('0')).toUpper();
        if (i % 16 == 15) hexStr += "\n";
    }
    if (data.size() > len) hexStr += "\n... (Data truncated for view)";
    
    hexView->setText(hexStr);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    // 判断事件源是否是图像预览区域
    if (obj == imageLabel || obj == imageScroll) {
        if (event->type() == QEvent::Wheel) {
            QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
            
            // 只有按下 Ctrl 键时才缩放，否则保持默认的滚动行为
            if (wheelEvent->modifiers() & Qt::ControlModifier) {
                const int delta = wheelEvent->angleDelta().y();
                
                if (delta > 0) {
                    m_scaleFactor *= 1.1; // 放大 10%
                } else {
                    m_scaleFactor /= 1.1; // 缩小 10%
                }

                // 限制缩放范围 (例如 10% 到 5000%)
                if (m_scaleFactor < 0.1) m_scaleFactor = 0.1;
                if (m_scaleFactor > 50.0) m_scaleFactor = 50.0;

                refreshImagePreview(); // 应用缩放
                return true; // 事件已处理，不再传递
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// 负责从数据加载原始图像
void MainWindow::updateImageView(TreeItem *item) {
    // 清空状态
    m_sourceImage = QImage(); 
    imageLabel->clear();
    imageLabel->setText("No Image Preview");
    lblZoom->setText("-");

    if (item->m_type != ImageChunkType) return;
    if (item->cachedData.isEmpty()) return;

    int pixelCount = item->imgWidth * item->imgHeight;
    if (pixelCount == 0) return;
    
    const uchar* rawData = (const uchar*)item->cachedData.constData();
    
    // 生成 QImage (包含之前的行对齐修复)
    if (item->size >= pixelCount * 2) {
        m_sourceImage = QImage(rawData, item->imgWidth, item->imgHeight, item->imgWidth * 2, QImage::Format_RGB16);
    } else if (item->size >= pixelCount) {
        m_sourceImage = QImage(rawData, item->imgWidth, item->imgHeight, item->imgWidth, QImage::Format_Indexed8);
        QVector<QRgb> colorTable;
        for (int i = 0; i < 256; i++) colorTable.push_back(qRgb(i, i, i));
        m_sourceImage.setColorTable(colorTable);
    } else {
        imageLabel->setText("Data size mismatch.");
        return;
    }

    if (!m_sourceImage.isNull()) {
        // 深拷贝一份数据，因为 cachedData 指针可能会变动或不安全（虽然在View里一般是安全的，但深拷贝更稳妥）
        m_sourceImage = m_sourceImage.copy(); 
        
        // 重置缩放比例
        m_scaleFactor = 1.0; 
        
        // 第一次显示
        refreshImagePreview();
    }
}

// 负责根据缩放比例显示图像
void MainWindow::refreshImagePreview() {
    if (m_sourceImage.isNull()) return;

    // 计算新的尺寸
    QSize newSize = m_sourceImage.size() * m_scaleFactor;
    
    // 缩放图像
    // Qt::FastTransformation (邻近插值) 对于像素图非常重要，保持边缘锐利
    // Qt::SmoothTransformation 会让像素图变糊
    QPixmap pix = QPixmap::fromImage(m_sourceImage.scaled(newSize, Qt::KeepAspectRatio, Qt::FastTransformation));
    
    imageLabel->setPixmap(pix);
    imageLabel->resize(pix.size()); // 确保 Label 大小跟随图片变化
    
    // 更新 UI 显示
    lblZoom->setText(QString::asprintf("%.0f%%", m_scaleFactor * 100));
}

void MainWindow::extractItem() {
    QModelIndex index = treeView->currentIndex();
    if (!index.isValid()) return;
    
    TreeItem *item = model->getItem(index);
    QString path = QFileDialog::getSaveFileName(this, "Save Data");
    if (path.isEmpty()) return;

    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        if (!item->cachedData.isEmpty()) {
            f.write(item->cachedData);
        } else {
            // 从源文件拷贝
            QFile *src = model->getFileHandle();
            src->seek(item->fileOffset);
            // 分块复制
            qint64 remain = item->size;
            char buf[4096];
            while(remain > 0) {
                int read = src->read(buf, qMin(remain, (qint64)4096));
                f.write(buf, read);
                remain -= read;
            }
        }
        f.close();
        QMessageBox::information(this, "Success", "Data extracted successfully.");
    }
}

// ==========================================
// 4. Main 入口
// ==========================================

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.show();
    return app.exec();
}

#include "main.moc"

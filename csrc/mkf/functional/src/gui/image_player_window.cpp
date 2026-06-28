#include "gui/image_player_window.h"
#include "gui/main_window.h"
#include "core/io/parse_flic.h"
#include "core/utils/flic.h"
#include <QVBoxLayout>

ImagePlayerWindow::ImagePlayerWindow(MainWindow* mainWindow) : QWidget(nullptr, Qt::Window), m_mainWindow(mainWindow) {
    setupUI();
}

ImagePlayerWindow::~ImagePlayerWindow() {
}

void ImagePlayerWindow::setupUI() {
    setWindowTitle("Image Player");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    imagePlayerWidget = new ImagePlayerWidget(this);
    mainLayout->addWidget(imagePlayerWidget);
}

void ImagePlayerWindow::update(const QModelIndex &index) {
    int depth = indexDepth(index);
    if (depth <= 0 || depth >= 3) {
        return;
    }
    int row = depth == 1 ? index.row() : index.parent().row();
    ResourceModel* resourceModel = m_mainWindow->getResourceModel();
    QString type = resourceModel->getType(row);
    QByteArray bytes = resourceModel->getResource(row);
    if (type.startsWith("FLC")) {
        imagePlayerWidget->setSpeed(parseFPS(bytes));
        std::vector<QImage> images = parseFLIC(bytes);
        if (images.empty()) {
            return;
        }
        imagePlayerWidget->setImages(images);
    } else if (type.startsWith("SPR") || type.startsWith("SMP")) {
        std::vector<GraphInfo> graphInfos = parseGraphInfos(bytes);
        std::vector<QImage> images = parseImages(bytes);
        if (images.empty()) {
            return;
        }
        imagePlayerWidget->setImages(images);
        imagePlayerWidget->setGraphInfos(graphInfos);
    } else if (type.startsWith("!") || type.startsWith("$")) {
        bool isGrayscale = type.startsWith("$");
        // #region Repeated code GraphicsTextWindow::displayRawImage
        QString type = resourceModel->getType(row);
        auto wxh = parseWXH(type);
        int width = wxh.first.width;
        int height = wxh.first.height;
        QString msg = wxh.second;
        if (!msg.isEmpty()) {
            emit statusMessage(msg);
            return;
        }
        QByteArray bytes = resourceModel->getResource(row);
        uint expectedSize = isGrayscale ? width * height : width * height * 2;
        if (bytes.size() != expectedSize) {
            QString message = QString("%1 x %2 %3 != %4").arg(width).arg(height).arg(isGrayscale ? "" : "x 2").arg(bytes.size());
            emit statusMessage(message);
            return;
        }
        QImage image = parseImage(bytes, width, height,
            isGrayscale ? QImage::Format_Grayscale8 : QImage::Format_RGB555, isGrayscale);
        // #endregion Repeated code GraphicsTextWindow::displayRawImage
        imagePlayerWidget->setImages({image});
    }
}

void ImagePlayerWindow::onTreeRowChanged(const QModelIndex &index) {
    imagePlayerWidget->setImages({});
    imagePlayerWidget->setGraphInfos({});
    if (!index.isValid()) {
        return;
    }
    update(index);
}
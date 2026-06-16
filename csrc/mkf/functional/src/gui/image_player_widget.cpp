#include "gui/image_player_widget.h"
#include <QPainter>
#include <QIcon>

ImagePlayerWidget::ImagePlayerWidget(QWidget* parent)
    : QWidget(parent),
      m_currentFrame(0),
      m_startFrame(0),
      m_endFrame(0),
      m_isPlaying(false),
      m_loop(true),
      m_fps(15),
      m_timer(new QTimer(this)),
      m_canvasWidth(0),
      m_canvasHeight(0),
      m_offsetX(0),
      m_offsetY(0),
      m_view(nullptr),
      m_scene(nullptr),
      m_pixmapItem(nullptr),
      m_playPauseBtn(nullptr),
      m_startFrameSpin(nullptr),
      m_currentFrameSpin(nullptr),
      m_endFrameSpin(nullptr),
      m_loopCheckBox(nullptr) {
    setupUI();
}

ImagePlayerWidget::~ImagePlayerWidget() {
}

void ImagePlayerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_view = new QGraphicsView(this);
    m_scene = new QGraphicsScene(m_view);
    m_view->setScene(m_scene);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    m_view->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    mainLayout->addWidget(m_view);

    QWidget* controlBar = new QWidget(this);
    QHBoxLayout* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(8, 4, 8, 4);
    controlLayout->setSpacing(8);

    m_playPauseBtn = new QPushButton(controlBar);
    m_playPauseBtn->setText("Play");
    controlLayout->addWidget(m_playPauseBtn);

    m_startFrameSpin = new QSpinBox(controlBar);
    m_startFrameSpin->setSingleStep(5);
    m_startFrameSpin->setMinimum(0);
    m_startFrameSpin->setMaximum(0);
    controlLayout->addWidget(m_startFrameSpin);

    m_currentFrameSpin = new QSpinBox(controlBar);
    m_currentFrameSpin->setSingleStep(1);
    m_currentFrameSpin->setMinimum(0);
    m_currentFrameSpin->setMaximum(0);
    controlLayout->addWidget(m_currentFrameSpin);

    m_endFrameSpin = new QSpinBox(controlBar);
    m_endFrameSpin->setSingleStep(5);
    m_endFrameSpin->setMinimum(0);
    m_endFrameSpin->setMaximum(0);
    controlLayout->addWidget(m_endFrameSpin);

    m_loopCheckBox = new QCheckBox("Loop", controlBar);
    m_loopCheckBox->setChecked(true);
    controlLayout->addWidget(m_loopCheckBox);

    mainLayout->addWidget(controlBar);

    connect(m_playPauseBtn, &QPushButton::clicked,
            this, &ImagePlayerWidget::onPlayPauseClicked);
    connect(m_startFrameSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ImagePlayerWidget::onStartFrameChanged);
    connect(m_endFrameSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ImagePlayerWidget::onEndFrameChanged);
    connect(m_currentFrameSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ImagePlayerWidget::onCurrentFrameChanged);
    connect(m_timer, &QTimer::timeout,
            this, &ImagePlayerWidget::onTimerTick);
}

void ImagePlayerWidget::setImages(const std::vector<QImage>& images) {
    m_images = images;
    m_startFrame = 0;
    m_endFrame = static_cast<int>(m_images.size()) - 1;
    m_currentFrame = 0;

    m_startFrameSpin->setMaximum(m_endFrame);
    m_endFrameSpin->setMaximum(m_endFrame);
    m_currentFrameSpin->setMaximum(m_endFrame);
    m_currentFrameSpin->setMinimum(m_startFrame);

    m_startFrameSpin->setValue(0);
    m_endFrameSpin->setValue(m_endFrame);
    m_currentFrameSpin->setValue(0);

    calculateBoundingBox();
    updateDisplay();
}

void ImagePlayerWidget::setGraphInfos(const std::vector<GraphInfo>& infos) {
    m_graphInfos = infos;
    calculateBoundingBox();
    updateDisplay();
}

void ImagePlayerWidget::calculateBoundingBox() {
    if (m_images.empty()) {
        m_canvasWidth = 0;
        m_canvasHeight = 0;
        m_offsetX = 0;
        m_offsetY = 0;
        return;
    }

    int minX = INT_MAX, minY = INT_MAX;
    int maxX = INT_MIN, maxY = INT_MIN;

    for (size_t i = 0; i < m_images.size(); ++i) {
        int x = 0, y = 0;
        if (i < m_graphInfos.size()) {
            x = m_graphInfos[i].x;
            y = m_graphInfos[i].y;
        }
        int w = m_images[i].width();
        int h = m_images[i].height();

        minX = qMin(minX, x);
        minY = qMin(minY, y);
        maxX = qMax(maxX, x + w);
        maxY = qMax(maxY, y + h);
    }

    m_canvasWidth = maxX - minX;
    m_canvasHeight = maxY - minY;
    m_offsetX = -minX;
    m_offsetY = -minY;
}

QImage ImagePlayerWidget::compositeFrame(int index) {
    if (index < 0 || index >= static_cast<int>(m_images.size())) {
        return QImage();
    }

    const QImage& img = m_images[index];

    if (m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        return img;
    }

    QImage canvas(m_canvasWidth, m_canvasHeight, QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);

    QPainter painter(&canvas);
    int x = 0, y = 0;
    if (static_cast<size_t>(index) < m_graphInfos.size()) {
        x = m_graphInfos[index].x;
        y = m_graphInfos[index].y;
    }

    painter.drawImage(x + m_offsetX, y + m_offsetY, img);
    return canvas;
}

void ImagePlayerWidget::updateDisplay() {
    if (m_currentFrame < 0 || m_currentFrame >= static_cast<int>(m_images.size())) {
        return;
    }

    QImage frame = compositeFrame(m_currentFrame);
    QPixmap pixmap = QPixmap::fromImage(frame);

    if (!m_pixmapItem) {
        m_pixmapItem = m_scene->addPixmap(pixmap);
    } else {
        m_pixmapItem->setPixmap(pixmap);
    }

    m_scene->setSceneRect(0, 0, frame.width(), frame.height());
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);

    m_currentFrameSpin->blockSignals(true);
    m_currentFrameSpin->setValue(m_currentFrame);
    m_currentFrameSpin->blockSignals(false);
}

void ImagePlayerWidget::play() {
    if (m_images.empty()) return;
    m_isPlaying = true;
    m_playPauseBtn->setText("Pause");
    m_timer->start(1000 / m_fps);
}

void ImagePlayerWidget::pause() {
    m_isPlaying = false;
    m_playPauseBtn->setText("Play");
    m_timer->stop();
}

void ImagePlayerWidget::stop() {
    pause();
    seek(m_startFrame);
}

void ImagePlayerWidget::seek(int frameIndex) {
    if (frameIndex < m_startFrame) frameIndex = m_startFrame;
    if (frameIndex > m_endFrame) frameIndex = m_endFrame;
    m_currentFrame = frameIndex;
    updateDisplay();
    emit frameChanged(m_currentFrame);
}

void ImagePlayerWidget::setSpeed(int fps) {
    m_fps = qMax(1, fps);
    if (m_isPlaying) {
        m_timer->start(1000 / m_fps);
    }
}

void ImagePlayerWidget::setLoop(bool enabled) {
    m_loop = enabled;
    m_loopCheckBox->setChecked(enabled);
}

void ImagePlayerWidget::onTimerTick() {
    if (m_images.empty()) return;

    m_currentFrame++;
    if (m_currentFrame > m_endFrame) {
        if (m_loop) {
            m_currentFrame = m_startFrame;
        } else {
            m_currentFrame = m_endFrame;
            pause();
        }
    }

    updateDisplay();
    emit frameChanged(m_currentFrame);
}

void ImagePlayerWidget::onPlayPauseClicked() {
    if (m_isPlaying) {
        pause();
    } else {
        play();
    }
}

void ImagePlayerWidget::onStartFrameChanged(int value) {
    m_startFrame = value;
    if (m_startFrame > m_endFrame) {
        m_endFrameSpin->blockSignals(true);
        m_endFrameSpin->setValue(m_startFrame);
        m_endFrameSpin->blockSignals(false);
        m_endFrame = m_startFrame;
    }
    m_currentFrameSpin->setMinimum(m_startFrame);
    if (m_currentFrame < m_startFrame) {
        seek(m_startFrame);
    }
}

void ImagePlayerWidget::onEndFrameChanged(int value) {
    m_endFrame = value;
    if (m_endFrame < m_startFrame) {
        m_startFrameSpin->blockSignals(true);
        m_startFrameSpin->setValue(m_endFrame);
        m_startFrameSpin->blockSignals(false);
        m_startFrame = m_endFrame;
    }
    m_currentFrameSpin->setMaximum(m_endFrame);
    if (m_currentFrame > m_endFrame) {
        seek(m_endFrame);
    }
}

void ImagePlayerWidget::onCurrentFrameChanged(int value) {
    if (value != m_currentFrame) {
        seek(value);
    }
}

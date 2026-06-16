#pragma once

#include "core/types/graph_info.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QWidget>
#include <QTimer>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <vector>

class ImagePlayerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ImagePlayerWidget(QWidget* parent = nullptr);
    ~ImagePlayerWidget();

    void setImages(const std::vector<QImage>& images);
    void setGraphInfos(const std::vector<GraphInfo>& infos);

    void play();
    void pause();
    void stop();
    void seek(int frameIndex);

    void setSpeed(int fps);
    void setLoop(bool enabled);

signals:
    void frameChanged(int index);

private slots:
    void onTimerTick();
    void onPlayPauseClicked();
    void onStartFrameChanged(int value);
    void onEndFrameChanged(int value);
    void onCurrentFrameChanged(int value);

private:
    void setupUI();
    void calculateBoundingBox();
    QImage compositeFrame(int index);
    void updateDisplay();

    std::vector<QImage> m_images;
    std::vector<GraphInfo> m_graphInfos;

    int m_currentFrame;
    int m_startFrame;
    int m_endFrame;
    bool m_isPlaying;
    bool m_loop;
    int m_fps;
    QTimer* m_timer;

    int m_canvasWidth;
    int m_canvasHeight;
    int m_offsetX;
    int m_offsetY;

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;

    QPushButton* m_playPauseBtn;
    QSpinBox* m_startFrameSpin;
    QSpinBox* m_currentFrameSpin;
    QSpinBox* m_endFrameSpin;
    QCheckBox* m_loopCheckBox;
};

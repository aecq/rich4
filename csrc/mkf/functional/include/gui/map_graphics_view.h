#pragma once
#include <QGraphicsView>
#include <QKeyEvent>

class MapGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MapGraphicsView(QWidget* parent = nullptr);
    void wheelEvent(QWheelEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

signals:
    void mousePositionChanged(int x, int y);
    void northRotateBy(int delta);
};
#pragma once
#include <QGraphicsView>

class MapGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    explicit MapGraphicsView(QWidget* parent = nullptr);
    void wheelEvent(QWheelEvent* event) override;
};
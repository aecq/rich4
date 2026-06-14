#include "gui/map_graphics_view.h"
#include <QWheelEvent>

MapGraphicsView::MapGraphicsView(QWidget* parent) : QGraphicsView(parent) {}

void MapGraphicsView::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0/1.15;
        scale(factor, factor);
        event->accept();
        return;
    }
    QGraphicsView::wheelEvent(event);
}

void MapGraphicsView::mouseMoveEvent(QMouseEvent* event) {
    QPointF scenePos = mapToScene(event->pos());
    emit mousePositionChanged(scenePos.x(), scenePos.y());
    QGraphicsView::mouseMoveEvent(event);
}
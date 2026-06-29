#include "gui/map_graphics_view.h"
#include <QWheelEvent>

MapGraphicsView::MapGraphicsView(QWidget* parent) : QGraphicsView(parent) {
    setFocusPolicy(Qt::StrongFocus);
}

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

void MapGraphicsView::keyPressEvent(QKeyEvent* event) {
    switch(event->key()) {
        case Qt::Key_Comma:
            emit northRotateBy(-1);
            event->accept();
            return;
        case Qt::Key_Period:
            emit northRotateBy(+1);
            event->accept();
            return;
        default:
            break;
    }
    QGraphicsView::keyPressEvent(event);
}
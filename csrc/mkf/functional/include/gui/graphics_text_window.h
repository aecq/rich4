#pragma once

#include "core/types/map.h"
#include "core/utils/resource_model.h"
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QImage>
#include <QListWidget>
#include <QModelIndex>
#include <QTextEdit>
#include <QWidget>
#include <vector>
class MainWindow;

class GraphicsTextWindow : public QWidget {
    Q_OBJECT
public:
    explicit GraphicsTextWindow(MainWindow* mainWindow);
    ~GraphicsTextWindow();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUI();
    void update(const QModelIndex &index);
    QString paletteHTML(const QModelIndex &index, ResourceModel* resourceModel);

public slots:
    void onTreeRowChanged(const QModelIndex &index);
    void onGalleryContextMenu(const QPoint& pos);

private:
    MainWindow* m_mainWindow;
    std::vector<QImage> m_images;
    std::vector<MapNode> m_mapNodes;

    QTextEdit* textEdit;
    QListWidget* gallery;
    QGraphicsView* mapView;
    QGraphicsScene* mapScene;

signals:
    void statusMessage(const QString& message);
};
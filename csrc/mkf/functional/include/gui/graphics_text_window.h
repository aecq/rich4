#pragma once

#include "gui/map_panel_widget.h"
#include "core/utils/resource_model.h"
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

private:
    void setupUI();
    void update(const QModelIndex &index);
    QString paletteHTML(const QModelIndex &index, ResourceModel* resourceModel);

    void displayRawImage(const QModelIndex& index, ResourceModel* resourceModel, bool isGrayscale = false);
    void displayImages();

public slots:
    void onTreeRowChanged(const QModelIndex &index);
    void onGalleryContextMenu(const QPoint& pos);
    void onMapTextReady(const QString& text);

private:
    MainWindow* m_mainWindow;
    std::vector<QImage> m_images;

    QTextEdit* textEdit;
    QListWidget* gallery;
    MapPanelWidget* mapPanel;

signals:
    void statusMessage(const QString& message);
};
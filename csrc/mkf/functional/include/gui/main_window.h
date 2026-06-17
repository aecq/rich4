#pragma once

// #include "core/utils/Cache.h"
#include "core/utils/resource_model.h"
#include <QFile>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTreeView>
#include <miniaudio.h>
class GraphicsTextWindow;
class ImagePlayerWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void treeSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    // Cache* getCache() { return cache; }
    ResourceModel* getResourceModel() { return resourceModel; }
    QTreeView* getTreeView() { return treeView; }
    
private slots:
    void openFile();
    void playAudio();
    void saveCSV();
    void exportResource();
    void replaceResource();
    void openGraphicsTextWindow();
    void openImagePlayerWindow();
    void onTreeDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    void setupUI();
    void createMenuBar();
    void createToolBar();
    void setupConnections();
    void loadFileTree();

    void updatePlayActionState();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    ma_engine engine;
    bool engine_initialized = false;
    // Cache* cache = nullptr;
    ResourceModel* resourceModel = nullptr;
    QTreeView* treeView = nullptr;
    QStandardItemModel* treeModel = nullptr;

    QAction* playAudioAction = nullptr;
    QAction* playImagesAction = nullptr;
    QAction* saveCSVAction = nullptr;

    GraphicsTextWindow* graphicsTextWindow = nullptr;
    ImagePlayerWindow* imagePlayerWindow = nullptr;

signals:
    void treeRowChanged(const QModelIndex &index);
};
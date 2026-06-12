#pragma once

// #include "core/utils/Cache.h"
#include "core/utils/resource_model.h"
#include <QFile>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTreeView>
class GraphicsTextWindow;

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
    void playFLC();
    void saveCSV();
    void exportResource();
    void replaceResource();
    void openGraphicsTextWindow();
    void onTreeDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    void setupUI();
    void createMenuBar();
    void createToolBar();
    void setupConnections();
    void loadFileTree();

    void updatePlayActionState();

private:
    // Cache* cache = nullptr;
    ResourceModel* resourceModel = nullptr;
    QTreeView* treeView = nullptr;
    QStandardItemModel* treeModel = nullptr;

    QAction* playAudioAction = nullptr;
    QAction* playFLCAction = nullptr;
    QAction* saveCSVAction = nullptr;

    GraphicsTextWindow* graphicsTextWindow = nullptr;

signals:
    void treeRowChanged(const QModelIndex &index);
};
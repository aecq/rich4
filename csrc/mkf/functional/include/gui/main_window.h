#pragma once

// #include "core/utils/Cache.h"
#include "core/utils/ResourceModel.h"
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
    
private slots:
    void openFile();
    void playAudio();
    void openGraphicsTextWindow();
    
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

    GraphicsTextWindow* graphicsTextWindow = nullptr;

signals:
    void treeRowChanged(const QModelIndex &index);
};
#pragma once

#include "core/utils/Cache.h"
#include <QFile>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTreeView>

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void treeSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    
private slots:
    void openFile();
    void playAudio();
    
private:
    void setupUI();
    void createMenuBar();
    void createToolBar();
    void setupConnections();
    void loadFileTree();

    void updatePlayActionState();

private:
    Cache* cache = nullptr;
    QTreeView* treeView = nullptr;
    QStandardItemModel* treeModel = nullptr;

    QAction* playAudioAction = nullptr;
};
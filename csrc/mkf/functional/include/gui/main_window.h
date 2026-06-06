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
    
private slots:
    void openFile();
    
private:
    void setupUI();
    void createMenuBar();
    void createToolBar();
    void loadFileTree();

private:
    Cache* cache = nullptr;
    QTreeView* treeView = nullptr;
    QStandardItemModel* treeModel = nullptr;
};
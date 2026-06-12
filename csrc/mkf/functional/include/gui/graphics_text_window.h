#pragma once

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

public slots:
    void onTreeRowChanged(const QModelIndex &index);
    void onGalleryContextMenu(const QPoint& pos);

private:
    MainWindow* m_mainWindow;
    std::vector<QImage> m_images;

    QTextEdit* textEdit;
    QListWidget* gallery;

signals:
    void statusMessage(const QString& message);
};
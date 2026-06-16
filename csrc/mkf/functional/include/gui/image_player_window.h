#pragma once

#include "gui/image_player_widget.h"
class MainWindow;

class ImagePlayerWindow : public QWidget {
    Q_OBJECT
public:
    explicit ImagePlayerWindow(MainWindow* mainWindow);
    ~ImagePlayerWindow();

private:
    void setupUI();
    void update(const QModelIndex &index);

public slots:
    void onTreeRowChanged(const QModelIndex &index);

private:
    MainWindow* m_mainWindow;
    ImagePlayerWidget* imagePlayerWidget;

signals:
    void statusMessage(const QString& message);
};
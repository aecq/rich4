#pragma once

#include "core/utils/resource_model.h"
#include <QListWidget>
#include <QModelIndex>
#include <QTextEdit>
#include <QWidget>
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

private:
    MainWindow* m_mainWindow;

    QTextEdit* textEdit;
    QListWidget* gallery;

signals:
    void statusMessage(const QString& message);
};
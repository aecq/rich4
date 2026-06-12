#pragma once

#include "core/utils/cache.h"
#include "core/io/parse.h"
#include <QObject>

class ResourceModel : public QObject {
    Q_OBJECT
public:
    ResourceModel();
    ~ResourceModel();
    void init(const QString& filename);
    void clear();
    ResourceOffset getOffset(int index);
    ResourceHeader getHeader(int index);
    QString getSignature(int index);
    QByteArray getResource(int index);
    size_t n();

    QString getMKFFolder();
    QString getCSVFolder();
    QString getBasename();
    QString getFilenamePrefix();  // folder/basename without .ext
    QString getCSVFilename();

    void setType(int index, QString);
    void setComment(int index, QString);

    void loadCSV();
    void saveCSV();

    QString getType(int index);
    QString getComment(int index);

    void exportBinary(int i, QString filename);

private:
    QString filenamePrefix;
    Cache* cache = nullptr;
    std::vector<QString> types;
    std::vector<QString> comments;

signals:
    void dataLoaded();
    void saved(const QString& message);
};
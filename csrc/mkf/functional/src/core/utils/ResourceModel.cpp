#include "core/utils/ResourceModel.h"

#include <QDebug>
#include <QDir>
#include <QFile>

ResourceModel::ResourceModel() {
}

ResourceModel::~ResourceModel() {
    delete cache;
}

void ResourceModel::init(const QString& filename) {
    if (!filename.endsWith(".mkf")) {
        qDebug() << "init: filename must end with .mkf";
        return;
    }
    clear();
    filenamePrefix = filename.left(filename.lastIndexOf("."));
    qDebug() << "ResourceModel::init filenamePrefix" << filenamePrefix;
    cache = new Cache(filename);
    loadCSV();
    emit dataLoaded();
}

void ResourceModel::clear() {
    if (cache) {
        cache->clear();
    }
    types.clear();
    comments.clear();
}

ResourceHeader ResourceModel::getHeader(int index) { return cache->getHeader(index); }

QString ResourceModel::getSignature(int index) { return cache->getSignature(index); }

QByteArray ResourceModel::getResource(int index) { return cache->getResource(index); }

size_t ResourceModel::n() { return cache->n(); }

QString ResourceModel::getMKFFolder() {
    if (filenamePrefix.lastIndexOf("/") == -1) {
        return QString();
    }
    return filenamePrefix.left(filenamePrefix.lastIndexOf("/"));
}

QString ResourceModel::getCSVFolder() {
    return getMKFFolder() + "/" + ".mkfcsv";
}

QString ResourceModel::getBasename() {
    if (filenamePrefix.lastIndexOf("/") == -1) {
        return QString();
    }
    return filenamePrefix.right(filenamePrefix.size() - filenamePrefix.lastIndexOf("/") - 1);
}

QString ResourceModel::getFilenamePrefix() { return filenamePrefix; }

QString ResourceModel::getCSVFilename() {
    return getCSVFolder() + "/" + getBasename() + ".csv";
}

void ResourceModel::loadCSV() {
    if (!cache) {
        qDebug() << "loadCSV: cache is null";
        return;
    }
    QString csvFilename = getCSVFilename();
    QFile csv(csvFilename);
    if (!csv.exists()) {
        QDir dir(getCSVFolder());
        if (!dir.exists()) {
            dir.mkpath(getCSVFolder());
        }
        qDebug() << "loadCSV: csv file does not exist";
        csv.open(QIODevice::WriteOnly);
        csv.write("index,type,comment\n");
        for (int i = 0; i < cache->n(); i++) {
            csv.write(
            QString("%1,%2,%3\n")
                    .arg(i).arg(cache->getSignature(i)).arg("")
                    .toStdString().c_str()
            );
        }
        csv.close();
        qDebug() << "created " << csvFilename;
    }
    types.resize(cache->n());
    comments.resize(cache->n());
    csv.open(QIODevice::ReadOnly);
    csv.readLine();
    while (!csv.atEnd()) {
        QString line = csv.readLine();
        if (line.isEmpty()) {
            continue;
        }
        line = line.trimmed();
        QStringList fields = line.split(",");
        if (fields.size() != 3) {
            qDebug() << "loadCSV: invalid csv line";
            continue;
        }
        int index = fields[0].toInt();
        QString type = fields[1].trimmed();
        QString comment = fields[2].trimmed();
        comment.replace("<comma>", ",");
        types[index] = type;
        comments[index] = comment;
    }
}

void ResourceModel::saveCSV() {
    if (!cache) {
        qDebug() << "saveCSV: cache is null";
        return;
    }
    QString csvFilename = getCSVFilename();
    QFile csv(csvFilename);
    csv.open(QIODevice::WriteOnly);
    csv.write("index,type,comment\n");
    for (int i = 0; i < cache->n(); i++) {
        csv.write(
            QString("%1,%2,%3\n")
                    .arg(i)
                    .arg(types[i].trimmed())
                    .arg(comments[i].replace(",", "<comma>"))
                    .toStdString().c_str()
        );
    }
    csv.close();
    qDebug() << "saved " << csvFilename;
}

QString ResourceModel::getType(int index) { return types[index]; }

QString ResourceModel::getComment(int index) { return comments[index]; }

void ResourceModel::onTypeChanged(int index, QString type) {
    if (index >= types.size()) {
        qDebug() << "onTypeChanged: index out of range";
        return;
    }
    types[index] = type;
}

void ResourceModel::onCommentChanged(int index, QString comment) {
    if (index >= comments.size()) {
        qDebug() << "onCommentChanged: index out of range";
        return;
    }
    comments[index] = comment;
}
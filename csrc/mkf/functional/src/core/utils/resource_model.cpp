#include "core/utils/resource_model.h"
#include "core/io/parse.h"
#include "core/types/resource_header.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

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

ResourceOffset ResourceModel::getOffset(int index) { return cache->getOffset(index); }

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

void ResourceModel::setType(int index, QString type) {
    if (index < 0 || index >= cache->n()) {
        qDebug() << "setType: index out of range";
        return;
    }
    types[index] = type;
}

void ResourceModel::setComment(int index, QString comment) {
    if (index < 0 || index >= cache->n()) {
        qDebug() << "setComment: index out of range";
        return;
    }
    comments[index] = comment;
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
                    .arg(i).arg(guessType(i)).arg("")
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
    QString csvTmpFilename = csvFilename + "-tmp";
    QString csvBakFilename = csvFilename + "-bak";
    QFile csv(csvFilename);
    QFile csvTmp(csvTmpFilename);
    QFile csvBak(csvBakFilename);
    if (csvTmp.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&csvTmp);
        out.setGenerateByteOrderMark(true);
        out << "index,type,comment\n";
        for (int i = 0; i < cache->n(); i++) {
            QString line = QString("%1,%2,%3\n")
                           .arg(i)
                           .arg(types[i].trimmed())
                           .arg(comments[i].replace(",", "<comma>"));
            out << line;
        }
        out.flush();
        csvTmp.close();
        qDebug() << "saved (UTF-8) " << csvTmpFilename;
    } else {
        qDebug() << "saveCSV: failed to open file" << csvTmpFilename;
    }
    if (csvTmp.exists()) {
        csv.remove();
        csv.rename(csvBakFilename);
        csvTmp.rename(csvFilename);
        qDebug() << csvTmpFilename << " renamed to " << csvFilename;
    }
    emit saved("Saved " + csvFilename);
}

QString ResourceModel::getType(int index) { return types[index]; }

QString ResourceModel::getComment(int index) { return comments[index]; }

QString ResourceModel::guessType(int index) {
    if (index < 0 || index >= n()) {
        qDebug() << "guessType: index out of range";
        return QString();
    }
    QString sig = getSignature(index);
    if (sig != Cache::unknownSignature()) {
        return sig;
    }
    if (getResource(index).left(1024).contains(".FLC")) {
        return "FLC";
    }
    ResourceHeader header = getHeader(index);
    switch (header.uncompressed_size) {
        // Data 2 bytes per pixel
        case 80000:
            return QString("!200x200");
        case 194776:
            return QString("!388x251");
        case 84480:
            return QString("!165x256");
        case 614400:  // Data and jump
            return QString("!640x480");
        // Panel 1 byte per pixel
        case 4824:
            return QString("$72x67");
        case 307200:
            return QString("$640x480");
        case 24576:
            return QString("$128x192");
    }
    if (index >= 1 && getSignature(index - 1) == "GND") {
        return QString("MAP");
    }
    return Cache::unknownSignature();
}

void ResourceModel::exportBinary(int i, QString filename) {
    if (!cache) {
        qDebug() << "exportBinary: cache is null";
        return;
    }
    QFile binary(filename);
    if (!binary.open(QIODevice::WriteOnly)) {
        qDebug() << "exportBinary: failed to open file" << filename;
        return;
    }
    binary.write(cache->getResource(i));
    binary.close();
    qDebug() << getBasename() << "[" << i << "] exported " << filename;
}
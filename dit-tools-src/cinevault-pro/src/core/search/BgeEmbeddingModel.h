#pragma once

#include <QString>
#include <QVector>

#include <memory>

class BgeEmbeddingModel {
public:
    BgeEmbeddingModel();
    ~BgeEmbeddingModel();

    BgeEmbeddingModel(const BgeEmbeddingModel &) = delete;
    BgeEmbeddingModel &operator=(const BgeEmbeddingModel &) = delete;

    bool isAvailable(QString *errorMessage = nullptr) const;
    QVector<float> embedDocument(const QString &text, QString *errorMessage) const;
    QVector<float> embedQuery(const QString &text, QString *errorMessage) const;
    QString modelRoot() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

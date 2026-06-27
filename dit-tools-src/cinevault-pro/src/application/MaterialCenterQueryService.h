#pragma once

#include "domain/Entities.h"

#include <QObject>
#include <QVariantList>

class GlobalDatabaseManager;
class SearchEngine;

class MaterialCenterQueryService : public QObject {
    Q_OBJECT

public:
    explicit MaterialCenterQueryService(GlobalDatabaseManager *globalDatabaseManager,
                                        SearchEngine *searchEngine,
                                        QObject *parent = nullptr);

    QVariantList fetchProjectOptions() const;
    QVariantList fetchSourceOptions(const QString &projectUuid) const;
    QVector<GlobalVideoAsset> fetchAssets(const QString &keyword,
                                          const QString &projectUuid,
                                          const QString &sourceName,
                                          int analysisStatusFilter,
                                          int confirmationStatusFilter) const;
    VideoAnalysisDetail fetchDetail(const QString &videoKey) const;

private:
    GlobalDatabaseManager *m_globalDatabaseManager = nullptr;
    SearchEngine *m_searchEngine = nullptr;
};

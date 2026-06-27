#pragma once

#include <QString>

class SearchEngine {
public:
    QString buildLikePattern(const QString &keyword) const;
    QString buildFtsQuery(const QString &keyword) const;
};

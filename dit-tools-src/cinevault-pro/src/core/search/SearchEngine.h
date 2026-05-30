#pragma once

#include <QString>

class SearchEngine {
public:
    QString buildLikePattern(const QString &keyword) const;
};

#pragma once

#include <QString>

class Paths {
public:
    static QString appDataRoot();
    static QString projectsRoot();
    static QString cacheRoot();
    static QString configRoot();
    static QString logsRoot();
    static bool ensureBaseDirectories(QString *errorMessage);
};

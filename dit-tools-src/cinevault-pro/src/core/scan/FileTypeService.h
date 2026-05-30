#pragma once

#include "domain/Enums.h"

#include <QString>

class FileTypeService {
public:
    static AssetType classify(const QString &fileName);
};

#pragma once

#include <QObject>
#include <QString>

class VolumeEjectService : public QObject {
    Q_OBJECT

public:
    explicit VolumeEjectService(QObject *parent = nullptr);

    bool ejectVolumeForPath(const QString &path, QString *message) const;
};

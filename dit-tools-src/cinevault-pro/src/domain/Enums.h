#pragma once

#include <QtGlobal>

enum class AssetType : qint32 {
    Unknown = 0,
    Video,
    Audio,
    Image,
    Subtitle,
    ProjectFile,
    Document,
    Archive,
    Other
};

enum class JobType : qint32 {
    Scan = 0,
    Metadata,
    Thumbnail,
    Report,
    Export
};

enum class JobState : qint32 {
    Pending = 0,
    Running,
    Completed,
    Failed,
    Cancelled
};

enum class WorkspaceId : qint32 {
    Import = 0,
    Library,
    Qc,
    Report,
    Jobs
};

enum class SelectionKind : qint32 {
    None = 0,
    SourceRoot,
    Folder,
    Asset
};

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
    GlobalSync,
    ContentAnalysis,
    Preview,
    Report,
    Export,
    Backup
};

enum class JobState : qint32 {
    Pending = 0,
    Running,
    Completed,
    Failed,
    Cancelled
};

enum class WorkspaceId : qint32 {
    ProjectLibrary = 0,
    Import,
    Library,
    MaterialCenter,
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

enum class ProbeStatus : qint32 {
    Pending = 0,
    Success,
    Unsupported,
    Unavailable,
    Failed
};

enum class MediaType : qint32 {
    Unknown = 0,
    Video,
    Audio,
    Image
};

enum class AnalysisMode : qint32 {
    EveryNFrames = 0,
    EveryFrame
};

enum class VideoAnalysisStatus : qint32 {
    Pending = 0,
    Running,
    Ready,
    Failed
};

enum class VideoAnalysisTaskStage : qint32 {
    Pending = 0,
    ExtractingFrames,
    AnalyzingFrames,
    Summarizing,
    Completed
};

enum class FrameAnalysisState : qint32 {
    Pending = 0,
    Success,
    Failed,
    Skipped
};

enum class AnalysisRunMode : qint32 {
    Initial = 0,
    Resume,
    Rebuild,
    SingleFrame
};

enum class ConfirmationStatus : qint32 {
    Pending = 0,
    Confirmed
};

enum class BackupSourceKind : qint32 {
    File = 0,
    Directory,
    Volume
};

enum class BackupVerificationMode : qint32 {
    Off = 0,
    Size,
    Sha256,
    Md5
};

enum class BackupTaskState : qint32 {
    Pending = 0,
    Running,
    Verifying,
    Completed,
    Warning,
    Failed,
    Cancelled
};

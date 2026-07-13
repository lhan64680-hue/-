#pragma once

#include "application/UpdaterSession.h"

#include <QWidget>

class QLabel;
class QProgressBar;
class QVBoxLayout;
class QCloseEvent;

class UpdaterWindow : public QWidget {
    Q_OBJECT

public:
    explicit UpdaterWindow(const UpdaterInstallSession &session, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QLabel *createStepChip(int index, const QString &label);
    void applyProgress(const UpdaterProgressEvent &event);
    void updateStepChips(int currentStep, const QString &accentColor);

    UpdaterInstallSession m_session;
    UpdaterSessionRunner *m_runner = nullptr;
    QLabel *m_iconLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_messageLabel = nullptr;
    QLabel *m_substepLabel = nullptr;
    QLabel *m_footerLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QList<QLabel *> m_stepChips;
    bool m_canClose = false;
};

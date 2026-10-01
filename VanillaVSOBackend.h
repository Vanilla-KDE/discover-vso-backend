/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KAuth/ExecuteJob>
#include <QPointer>
#include <resources/AbstractResourcesBackend.h>
#include <resources/StandardBackendUpdater.h>

class QFileSystemWatcher;
class VanillaVSOResource;

/**
 * Discover backend for Vanilla OS system image updates.
 *
 * Vanilla OS ships an immutable, atomically updated system image which is
 * managed by the `vso upgrade` command line tool. This backend exposes that
 * image to Discover as a single @c System resource.
 */
class VanillaVSOBackend : public AbstractResourcesBackend
{
    Q_OBJECT
public:
    explicit VanillaVSOBackend(QObject *parent = nullptr);

    int updatesCount() const override;
    AbstractBackendUpdater *backendUpdater() const override;
    AbstractReviewsBackend *reviewsBackend() const override;
    ResultsStream *search(const AbstractResourcesBackend::Filters &search) override;
    bool isValid() const override;
    Transaction *installApplication(AbstractResource *app) override;
    Transaction *installApplication(AbstractResource *app, const AddonList &addons) override;
    Transaction *removeApplication(AbstractResource *app) override;
    int fetchingUpdatesProgress() const override;
    void checkForUpdates() override;
    QString displayName() const override;

private:
    void acquireFetching(bool fetching);
    void refreshRebootState();
    void parseCheckOutput(const QByteArray &output);

    QPointer<VanillaVSOResource> m_resource;
    StandardBackendUpdater *const m_updater;
    QFileSystemWatcher *const m_rebootWatcher;
    KAuth::ExecuteJob *m_checkJob = nullptr;
    bool m_fetching = false;
};

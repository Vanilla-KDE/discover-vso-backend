/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <resources/AbstractResource.h>

#include <QList>
#include <QString>

class VanillaVSOBackend;

/**
 * Description of a single package that was added, upgraded, downgraded or
 * removed as part of a Vanilla OS system image update.
 *
 * This mirrors the entries found in the "systemPackageDiff" and
 * "overlayPackageDiff" objects reported by `vso upgrade check --json`.
 */
struct VanillaVSOPackageChange {
    QString name;
    QString previousVersion;
    QString newVersion;
};

/**
 * The set of package changes that a system image update brings along.
 */
struct VanillaVSOPackageDiff {
    QList<VanillaVSOPackageChange> added;
    QList<VanillaVSOPackageChange> upgraded;
    QList<VanillaVSOPackageChange> downgraded;
    QList<VanillaVSOPackageChange> removed;

    [[nodiscard]] bool isEmpty() const
    {
        return added.isEmpty() && upgraded.isEmpty() && downgraded.isEmpty() && removed.isEmpty();
    }

    [[nodiscard]] int count() const
    {
        return added.count() + upgraded.count() + downgraded.count() + removed.count();
    }
};

/**
 * Represents the Vanilla OS system image as a Discover resource.
 *
 * Unlike application backends there is exactly one of these per system: the
 * atomic image managed by `vso upgrade`. It shows up in the "Updates"
 * view whenever a newer image is available.
 */
class VanillaVSOResource : public AbstractResource
{
    Q_OBJECT
public:
    explicit VanillaVSOResource(const QString &name, const QString &installedVersion, VanillaVSOBackend *parent);

    // AbstractResource interface
    QString appstreamId() const override;
    AbstractResource::State state() override;
    QVariant icon() const override;
    QString comment() override;
    QString name() const override;
    QString packageName() const override;
    bool hasCategory(const QString &category) const override;
    QJsonArray licenses() override;
    QString longDescription() override;
    QList<PackageState> addonsInformation() override;
    bool isRemovable() const override;
    QString availableVersion() const override;
    QString installedVersion() const override;
    QString origin() const override;
    QString section() override;
    quint64 size() override;
    AbstractResource::Type type() const override;
    QString author() const override;
    bool canExecute() const override;
    void invokeApplication() const override
    {
    }
    QUrl url() const override;
    QString sourceIcon() const override;
    QUrl homepage() override;
    QUrl helpURL() override;
    QUrl bugURL() override;
    QDate releaseDate() const override;

public Q_SLOTS:
    void fetchScreenshots() override;
    void fetchChangelog() override;

public:
    void setState(AbstractResource::State state);
    void setInstalledVersion(const QString &version);
    void setAvailableVersion(const QString &version);
    void setPackageDiff(const VanillaVSOPackageDiff &diff);

    /* @returns the number of packages that this update would change */
    [[nodiscard]] int changedPackagesCount() const
    {
        return m_packageDiff.count();
    }

private:
    /* Build a translatable HTML summary of the package changes */
    QString packageDiffDescription() const;

    const QString m_appstreamId;
    const QString m_name;
    QString m_installedVersion;
    QString m_availableVersion;
    VanillaVSOPackageDiff m_packageDiff;
    AbstractResource::State m_state = AbstractResource::Installed;
};

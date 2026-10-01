/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "VanillaVSOBackend.h"

#include "VanillaVSOAuthActionFactory.h"
#include "VanillaVSOResource.h"
#include "VanillaVSOTransaction.h"
#include "libdiscover_vanillavso_debug.h"

#include <KLocalizedString>
#include <KOSRelease>

#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

DISCOVER_BACKEND_PLUGIN(VanillaVSOBackend)

namespace
{
/* Environment variable to force the backend on for development purposes. */
constexpr auto s_developmentOverride = "DISCOVER_VANILLAVSO_DEVEL";

/**
 * abroot creates these lock files while an upgrade is running and keeps them
 * around until the machine has been rebooted, in which case no new transaction
 * may be started yet.
 *
 * @note These are the paths checked by `AreABRootTransactionsLocked()` in vso.
 */
QStringList rebootLockPaths()
{
    return {
        QStringLiteral("/tmp/ABSystem.Upgrade.lock"),
        QStringLiteral("/tmp/ABSystem.Upgrade.user.lock"),
    };
}

/**
 * `vso upgrade check --json` forwards the output of
 * `abroot upgrade --check-only`, which is a single JSON object. Be lenient and
 * try the whole output first and then each line, from the last one backwards,
 * in case a warning is printed before the payload.
 */
QJsonObject extractJsonObject(const QByteArray &output)
{
    const QByteArray trimmed = output.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument whole = QJsonDocument::fromJson(trimmed, &parseError);
    if (parseError.error == QJsonParseError::NoError && whole.isObject()) {
        return whole.object();
    }

    const QList<QByteArray> lines = trimmed.split('\n');
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QByteArray line = it->trimmed();
        if (!line.startsWith('{')) {
            continue;
        }

        const QJsonDocument candidate = QJsonDocument::fromJson(line, &parseError);
        if (parseError.error == QJsonParseError::NoError && candidate.isObject()) {
            return candidate.object();
        }
    }

    return {};
}

VanillaVSOPackageChange parsePackageChange(const QJsonObject &object)
{
    // These keys mirror the JSON tags of `diff.PackageDiff` from the differ
    // library, which is what abroot serialises.
    VanillaVSOPackageChange change;
    change.name = object.value(QStringLiteral("name")).toString();
    change.previousVersion = object.value(QStringLiteral("previous_version")).toString();
    change.newVersion = object.value(QStringLiteral("new_version")).toString();
    return change;
}

void readPackageList(const QJsonObject &diff, const QString &key, QList<VanillaVSOPackageChange> &out)
{
    const QJsonArray array = diff.value(key).toArray();
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            out << parsePackageChange(value.toObject());
        }
    }
}

VanillaVSOPackageDiff parsePackageDiff(const QJsonObject &checkResult)
{
    VanillaVSOPackageDiff diff;

    const QList<QJsonObject> diffs{
        checkResult.value(QStringLiteral("systemPackageDiff")).toObject(),
        checkResult.value(QStringLiteral("overlayPackageDiff")).toObject(),
    };

    for (const QJsonObject &packageDiff : diffs) {
        readPackageList(packageDiff, QStringLiteral("added"), diff.added);
        readPackageList(packageDiff, QStringLiteral("upgraded"), diff.upgraded);
        readPackageList(packageDiff, QStringLiteral("downgraded"), diff.downgraded);
        readPackageList(packageDiff, QStringLiteral("removed"), diff.removed);
    }

    return diff;
}
} // namespace

VanillaVSOBackend::VanillaVSOBackend(QObject *parent)
    : AbstractResourcesBackend(parent)
    , m_updater(new StandardBackendUpdater(this))
    , m_rebootWatcher(new QFileSystemWatcher(this))
{
    connect(m_updater, &StandardBackendUpdater::updatesCountChanged, this, &VanillaVSOBackend::updatesCountChanged);

    if (!isValid()) {
        qCDebug(VANILLAVSO_LOG) << "This system does not look like Vanilla OS, not starting the backend";
        return;
    }

    const KOSRelease osRelease;
    QString name = osRelease.prettyName();
    if (name.isEmpty()) {
        name = i18n("Vanilla OS");
    }
    m_resource = new VanillaVSOResource(name, osRelease.versionId(), this);

    // Watch the containing directories as well, so we notice when one of the
    // lock files is created.
    QStringList watchedDirectories;
    for (const QString &lockPath : rebootLockPaths()) {
        const QString directory = QFileInfo(lockPath).absolutePath();
        if (!watchedDirectories.contains(directory)) {
            watchedDirectories << directory;
        }
    }
    m_rebootWatcher->addPaths(watchedDirectories);
    connect(m_rebootWatcher, &QFileSystemWatcher::directoryChanged, this, &VanillaVSOBackend::refreshRebootState);
    connect(m_rebootWatcher, &QFileSystemWatcher::fileChanged, this, &VanillaVSOBackend::refreshRebootState);
    refreshRebootState();

    QTimer::singleShot(0, this, &VanillaVSOBackend::checkForUpdates);
}

bool VanillaVSOBackend::isValid() const
{
    if (qEnvironmentVariableIsSet(s_developmentOverride)) {
        return true;
    }

    const KOSRelease osRelease;
    if (osRelease.id() != QLatin1String("vanilla")) {
        return false;
    }

    return !QStandardPaths::findExecutable(QStringLiteral("vso")).isEmpty();
}

int VanillaVSOBackend::updatesCount() const
{
    return m_updater->updatesCount();
}

AbstractBackendUpdater *VanillaVSOBackend::backendUpdater() const
{
    return m_updater;
}

AbstractReviewsBackend *VanillaVSOBackend::reviewsBackend() const
{
    return nullptr;
}

int VanillaVSOBackend::fetchingUpdatesProgress() const
{
    return m_fetching ? 42 : 100;
}

QString VanillaVSOBackend::displayName() const
{
    const KOSRelease osRelease;
    const QString prettyName = osRelease.prettyName();
    return prettyName.isEmpty() ? i18n("Vanilla OS") : prettyName;
}

ResultsStream *VanillaVSOBackend::search(const AbstractResourcesBackend::Filters &filter)
{
    // We only provide system updates; everything else is handled by other backends.
    if (!m_resource || !filter.resourceUrl.isEmpty()) {
        return new ResultsStream(QStringLiteral("vanillavso-empty"), {});
    }

    if (m_resource->state() < filter.state) {
        return new ResultsStream(QStringLiteral("vanillavso-empty"), {});
    }

    if (!filter.search.isEmpty() && !m_resource->name().contains(filter.search, Qt::CaseInsensitive)) {
        return new ResultsStream(QStringLiteral("vanillavso-empty"), {});
    }

    return new ResultsStream(QStringLiteral("vanillavso"), {StreamResult(m_resource)});
}

Transaction *VanillaVSOBackend::installApplication(AbstractResource *app)
{
    auto *resource = qobject_cast<VanillaVSOResource *>(app);
    if (!resource) {
        qCWarning(VANILLAVSO_LOG) << "Cannot install" << app << "with the Vanilla OS backend";
        return nullptr;
    }

    auto *transaction = new VanillaVSOTransaction(resource, Transaction::InstallRole);
    connect(transaction, &VanillaVSOTransaction::needReboot, this, [this] {
        m_updater->setNeedsReboot(true);
    });
    connect(transaction, &Transaction::statusChanged, this, [this](Transaction::Status status) {
        if (status == Transaction::DoneStatus || status == Transaction::DoneWithErrorStatus) {
            // Refresh the available update information once the system image changed.
            QTimer::singleShot(0, this, &VanillaVSOBackend::checkForUpdates);
        }
    });
    return transaction;
}

Transaction *VanillaVSOBackend::installApplication(AbstractResource *app, const AddonList &addons)
{
    Q_UNUSED(addons)
    return installApplication(app);
}

Transaction *VanillaVSOBackend::removeApplication(AbstractResource *app)
{
    Q_UNUSED(app)
    // The system image is not something the user can uninstall.
    return nullptr;
}

void VanillaVSOBackend::checkForUpdates()
{
    if (!m_resource || m_fetching) {
        return;
    }

    acquireFetching(true);

    m_checkJob = VanillaVSOActionFactory::createCheckAction();
    if (!m_checkJob) {
        acquireFetching(false);
        Q_EMIT passiveMessage(i18n("Unable to check for Vanilla OS system updates: elevated privileges are not available."));
        return;
    }

    connect(m_checkJob, &KAuth::ExecuteJob::result, this, [this](KJob *job) {
        auto *reply = static_cast<KAuth::ExecuteJob *>(job);
        m_checkJob = nullptr;
        reply->deleteLater();

        if (reply->error() != 0) {
            qCWarning(VANILLAVSO_LOG) << "Checking for system updates failed:" << reply->errorString();
            Q_EMIT passiveMessage(i18n("Could not check for system updates: %1", reply->errorString()));
        } else {
            parseCheckOutput(reply->data().value(QStringLiteral("output")).toByteArray());
        }

        refreshRebootState();
        acquireFetching(false);
        Q_EMIT contentsChanged();
    });

    m_checkJob->start();
}

void VanillaVSOBackend::parseCheckOutput(const QByteArray &output)
{
    const QJsonObject checkResult = extractJsonObject(output);
    if (checkResult.isEmpty()) {
        qCWarning(VANILLAVSO_LOG) << "Could not parse the JSON output of 'vso upgrade check --json':" << output;
        Q_EMIT passiveMessage(i18n("Unable to interpret the response from the Vanilla OS update service."));
        return;
    }

    const VanillaVSOPackageDiff diff = parsePackageDiff(checkResult);
    m_resource->setPackageDiff(diff);

    // `hasUpdate` only reports a new system image; changes that affect the
    // user's overlay packages alone are reported through the package diff.
    const bool imageUpdate = checkResult.value(QStringLiteral("hasUpdate")).toBool(false);
    const bool hasUpdate = imageUpdate || !diff.isEmpty();

    if (!hasUpdate) {
        m_resource->setAvailableVersion(m_resource->installedVersion());
        m_resource->setState(AbstractResource::Installed);
        return;
    }

    // abroot does not expose a human readable version for the new image, so fall
    // back to the image digest and, when only overlay packages changed, to the
    // number of packages involved.
    QString newDigest = checkResult.value(QStringLiteral("newDigest")).toString();
    const int separator = newDigest.indexOf(QLatin1Char(':'));
    if (separator >= 0) {
        newDigest = newDigest.mid(separator + 1);
    }

    QString availableVersion = newDigest.left(12);
    if (availableVersion.isEmpty()) {
        availableVersion = i18np("%1 package update", "%1 package updates", diff.count());
    }

    m_resource->setAvailableVersion(availableVersion);
    m_resource->setState(AbstractResource::Upgradeable);
}

void VanillaVSOBackend::acquireFetching(bool fetching)
{
    if (m_fetching == fetching) {
        return;
    }

    m_fetching = fetching;
    Q_EMIT fetchingUpdatesProgressChanged();
}

void VanillaVSOBackend::refreshRebootState()
{
    bool needsReboot = false;
    const QStringList lockPaths = rebootLockPaths();
    for (const QString &lockPath : lockPaths) {
        if (QFile::exists(lockPath)) {
            needsReboot = true;
            break;
        }
    }

    if (needsReboot == m_updater->needsReboot()) {
        return;
    }

    m_updater->setNeedsReboot(needsReboot);
    if (needsReboot) {
        qCInfo(VANILLAVSO_LOG) << "A system upgrade is running or pending a reboot; another one cannot be started yet";
    }
}

#include "VanillaVSOBackend.moc"

#include "moc_VanillaVSOBackend.cpp"

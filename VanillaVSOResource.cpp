/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "VanillaVSOResource.h"

#include "VanillaVSOBackend.h"

#include <KLocalizedString>

#include <QJsonArray>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace
{
constexpr int s_maxListedPackages = 20;

QString changesToHtml(const QString &title, const QList<VanillaVSOPackageChange> &changes)
{
    if (changes.isEmpty()) {
        return {};
    }

    QStringList items;
    items.reserve(qMin(changes.count(), s_maxListedPackages));
    for (int i = 0; i < changes.count() && i < s_maxListedPackages; ++i) {
        const auto &change = changes.at(i);
        const QString name = change.name.toHtmlEscaped();
        if (change.previousVersion.isEmpty()) {
            items += QStringLiteral("<li>%1 (%2)</li>").arg(name, change.newVersion.toHtmlEscaped());
        } else if (change.newVersion.isEmpty()) {
            items += QStringLiteral("<li>%1 (%2)</li>").arg(name, change.previousVersion.toHtmlEscaped());
        } else {
            items += QStringLiteral("<li>%1 (%2 &rarr; %3)</li>").arg(name, change.previousVersion.toHtmlEscaped(), change.newVersion.toHtmlEscaped());
        }
    }
    if (changes.count() > s_maxListedPackages) {
        items += QStringLiteral("<li>&hellip;</li>");
    }

    return QStringLiteral("<p>%1</p><ul>%2</ul>").arg(title, items.join(QString()));
}
} // namespace

VanillaVSOResource::VanillaVSOResource(const QString &name, const QString &installedVersion, VanillaVSOBackend *parent)
    : AbstractResource(parent)
    , m_appstreamId(QStringLiteral("org.vanillaos.VanillaOS"))
    , m_name(name)
    , m_installedVersion(installedVersion)
    , m_availableVersion(installedVersion)
{
}

QString VanillaVSOResource::appstreamId() const
{
    return m_appstreamId;
}

QString VanillaVSOResource::packageName() const
{
    return m_appstreamId;
}

QString VanillaVSOResource::name() const
{
    return m_name;
}

QString VanillaVSOResource::comment()
{
    if (m_state == Upgradeable) {
        return i18n("A system update is available");
    }
    return i18n("Your system is up to date");
}

QVariant VanillaVSOResource::icon() const
{
    return QStringLiteral("system-software-update");
}

AbstractResource::State VanillaVSOResource::state()
{
    return m_state;
}

void VanillaVSOResource::setState(AbstractResource::State state)
{
    if (m_state == state) {
        return;
    }

    m_state = state;
    Q_EMIT stateChanged();
}

QString VanillaVSOResource::installedVersion() const
{
    return m_installedVersion;
}

void VanillaVSOResource::setInstalledVersion(const QString &version)
{
    if (m_installedVersion == version) {
        return;
    }

    m_installedVersion = version;
    Q_EMIT versionsChanged();
}

QString VanillaVSOResource::availableVersion() const
{
    return m_availableVersion;
}

void VanillaVSOResource::setAvailableVersion(const QString &version)
{
    if (m_availableVersion == version) {
        return;
    }

    m_availableVersion = version;
    Q_EMIT versionsChanged();
}

void VanillaVSOResource::setPackageDiff(const VanillaVSOPackageDiff &diff)
{
    m_packageDiff = diff;
    Q_EMIT longDescriptionChanged();
}

QString VanillaVSOResource::packageDiffDescription() const
{
    if (m_packageDiff.isEmpty()) {
        return {};
    }

    QString html;
    html += changesToHtml(i18n("Packages to upgrade:"), m_packageDiff.upgraded);
    html += changesToHtml(i18n("Packages to install:"), m_packageDiff.added);
    html += changesToHtml(i18n("Packages to downgrade:"), m_packageDiff.downgraded);
    html += changesToHtml(i18n("Packages to remove:"), m_packageDiff.removed);
    return html;
}

QString VanillaVSOResource::longDescription()
{
    if (m_state == Upgradeable) {
        return i18n("<p>A new version of the Vanilla OS system image is available. "
                    "The system will be upgraded and restarted to apply the changes.</p>%1",
                    packageDiffDescription());
    }
    return i18n("<p>Your Vanilla OS system image is up to date.</p>");
}

void VanillaVSOResource::fetchChangelog()
{
    Q_EMIT changelogFetched(longDescription());
}

void VanillaVSOResource::fetchScreenshots()
{
    Q_EMIT screenshotsFetched({});
}

QJsonArray VanillaVSOResource::licenses()
{
    return {QJsonObject{
        {QStringLiteral("name"), i18n("Various free software licenses")},
        {QStringLiteral("url"), QString()},
    }};
}

QList<PackageState> VanillaVSOResource::addonsInformation()
{
    return {};
}

bool VanillaVSOResource::hasCategory(const QString &category) const
{
    return category == QLatin1String("System") || category == QLatin1String("Operating System");
}

AbstractResource::Type VanillaVSOResource::type() const
{
    return System;
}

bool VanillaVSOResource::canExecute() const
{
    return false;
}

bool VanillaVSOResource::isRemovable() const
{
    return false;
}

quint64 VanillaVSOResource::size()
{
    return 0;
}

QString VanillaVSOResource::origin() const
{
    return i18n("Vanilla OS");
}

QString VanillaVSOResource::section()
{
    return {};
}

QString VanillaVSOResource::author() const
{
    return i18n("Vanilla OS");
}

QString VanillaVSOResource::sourceIcon() const
{
    return QStringLiteral("computer-symbolic");
}

QUrl VanillaVSOResource::url() const
{
    return QUrl(QStringLiteral("vanillavso://") + m_appstreamId, QUrl::StrictMode);
}

QUrl VanillaVSOResource::homepage()
{
    return QUrl(QStringLiteral("https://vanillaos.org"), QUrl::StrictMode);
}

QUrl VanillaVSOResource::helpURL()
{
    return QUrl(QStringLiteral("https://handbook.vanillaos.org"), QUrl::StrictMode);
}

QUrl VanillaVSOResource::bugURL()
{
    return QUrl(QStringLiteral("https://github.com/Vanilla-OS/vanilla-system-operator/issues"), QUrl::StrictMode);
}

QDate VanillaVSOResource::releaseDate() const
{
    return {};
}

/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "VanillaVSOKAuthHelper.h"

#include "libdiscover_vanillavso_helper_debug.h"

#include <KAuth/HelperSupport>

#include <QEventLoop>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

using namespace KAuth;

namespace
{
constexpr int s_processStartTimeout = 30 * 1000;

QString commandName()
{
    return QStringLiteral("vso");
}
} // namespace

VanillaVSOKAuthHelper::VanillaVSOKAuthHelper() = default;

VanillaVSOKAuthHelper::~VanillaVSOKAuthHelper() = default;

ActionReply VanillaVSOKAuthHelper::check(const QVariantMap &args)
{
    Q_UNUSED(args)
    // `vso upgrade check --json` forwards the JSON emitted by
    // `abroot upgrade --check-only`. abroot reports "no update available"
    // through a non-zero exit code, so never treat that as a failure here and
    // let the backend interpret the payload instead.
    return runVso({QStringLiteral("upgrade"), QStringLiteral("check"), QStringLiteral("--json")}, false);
}

ActionReply VanillaVSOKAuthHelper::upgrade(const QVariantMap &args)
{
    Q_UNUSED(args)
    // `--now` upgrades immediately instead of deferring to the configured update
    // schedule, which is what the user asked for by triggering the update.
    return runVso({QStringLiteral("upgrade"), QStringLiteral("--now")}, true);
}

ActionReply VanillaVSOKAuthHelper::runVso(const QStringList &arguments, bool reportError)
{
    m_stdout.clear();
    m_stderr.clear();
    m_lastProgress = 0;
    m_exitCode = -1;

    const QString executable = QStandardPaths::findExecutable(commandName());
    if (executable.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Unable to find the '%1' executable.").arg(commandName()));
        return reply;
    }

    QEventLoop loop;

    m_process = new QProcess(this);
    m_process->setProgram(executable);
    m_process->setArguments(arguments);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &VanillaVSOKAuthHelper::readStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &VanillaVSOKAuthHelper::readStandardError);
    connect(m_process, &QProcess::errorOccurred, this, [this, &loop](QProcess::ProcessError error) {
        m_stderr += QStringLiteral("QProcess error %1: %2").arg(int(error)).arg(m_process->errorString()).toUtf8();
        loop.quit();
    });
    connect(m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, &loop](int exitCode, QProcess::ExitStatus exitStatus) {
        m_stdout += m_process->readAllStandardOutput();
        m_stderr += m_process->readAllStandardError();
        m_exitCode = exitStatus == QProcess::NormalExit ? exitCode : -1;
        loop.quit();
    });

    HelperSupport::progressStep(0);
    m_process->start();
    if (!m_process->waitForStarted(s_processStartTimeout)) {
        const QString errorString = m_process->errorString();
        m_process->deleteLater();
        m_process = nullptr;

        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Unable to start '%1': %2").arg(commandName(), errorString));
        return reply;
    }

    loop.exec();

    m_process->deleteLater();
    m_process = nullptr;

    QVariantMap data;
    data.insert(QStringLiteral("output"), m_stdout);
    data.insert(QStringLiteral("exitCode"), m_exitCode);

    HelperSupport::progressStep(100);

    if (reportError && m_exitCode != 0) {
        QString description = QString::fromUtf8(m_stderr).trimmed();
        if (description.isEmpty()) {
            description = QString::fromUtf8(m_stdout).trimmed();
        }
        if (description.isEmpty()) {
            description = QStringLiteral("'%1 %2' failed with exit code %3.").arg(commandName(), arguments.join(QLatin1Char(' '))).arg(m_exitCode);
        }

        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(description);
        reply.setData({{QStringLiteral("errorString"), description}});
        return reply;
    }

    ActionReply reply = ActionReply::SuccessReply();
    reply.setData(data);
    return reply;
}

void VanillaVSOKAuthHelper::readStandardOutput()
{
    const QByteArray chunk = m_process->readAllStandardOutput();
    m_stdout += chunk;

    // `vso` prints a human readable progress log; pick up the last percentage
    // it reports so that Discover can show a progress bar.
    static const QRegularExpression percentageRegex(QStringLiteral("(\\d{1,3})\\s*%"));

    int lastPercentage = -1;
    auto matches = percentageRegex.globalMatch(QString::fromUtf8(chunk));
    while (matches.hasNext()) {
        lastPercentage = matches.next().captured(1).toInt();
    }

    if (lastPercentage > m_lastProgress && lastPercentage <= 100) {
        m_lastProgress = lastPercentage;
        HelperSupport::progressStep(m_lastProgress);
    }
}

void VanillaVSOKAuthHelper::readStandardError()
{
    m_stderr += m_process->readAllStandardError();
}

KAUTH_HELPER_MAIN("org.kde.discover.vanillavsobackend", VanillaVSOKAuthHelper)

/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KAuth/ActionReply>

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class QProcess;

/**
 * Privileged part of the Vanilla OS backend.
 *
 * This helper is executed as root through KAuth/PolicyKit and is the only place
 * where the `vso` command line tool is invoked. It replaces the `pkexec` calls
 * made by the original GNOME Software plugin.
 */
class VanillaVSOKAuthHelper : public QObject
{
    Q_OBJECT
public:
    VanillaVSOKAuthHelper();
    ~VanillaVSOKAuthHelper() override;

public Q_SLOTS:
    /**
     * Executes `vso upgrade check --json` and returns the raw stdout.
     * The action is registered as not requiring authentication.
     */
    KAuth::ActionReply check(const QVariantMap &args);

    /**
     * Executes `vso upgrade --now`. Requires administrative privileges.
     */
    KAuth::ActionReply upgrade(const QVariantMap &args);

private:
    KAuth::ActionReply runVso(const QStringList &arguments, bool reportError);
    void readStandardOutput();
    void readStandardError();

    QProcess *m_process = nullptr;
    QByteArray m_stdout;
    QByteArray m_stderr;
    int m_lastProgress = 0;
    int m_exitCode = -1;
};

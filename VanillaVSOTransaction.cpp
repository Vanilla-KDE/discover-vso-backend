/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "VanillaVSOTransaction.h"

#include "VanillaVSOAuthActionFactory.h"
#include "VanillaVSOBackend.h"
#include "VanillaVSOResource.h"
#include "libdiscover_vanillavso_debug.h"

#include <KLocalizedString>

VanillaVSOTransaction::VanillaVSOTransaction(VanillaVSOResource *resource, Transaction::Role role)
    : Transaction(resource->backend(), resource, role)
    , m_resource(resource)
{
    // The image is written atomically by vso; interrupting it halfway is not
    // something we want to offer to the user.
    setCancellable(false);
    setStatus(QueuedStatus);
    start();
}

void VanillaVSOTransaction::start()
{
    m_job = VanillaVSOActionFactory::createUpgradeAction();
    if (!m_job) {
        finishTransactionWithError(i18n("Unable to start the privileged action that upgrades the system."));
        return;
    }

    QObject::connect(m_job, &KAuth::ExecuteJob::result, this, [this](KJob *job) {
        auto *reply = static_cast<KAuth::ExecuteJob *>(job);
        m_job = nullptr;

        if (reply->error() == 0) {
            finishTransactionOK();
            return;
        }

        QString message = reply->data().value(QStringLiteral("errorString"), reply->errorString()).toString();
        if (reply->error() == KAuth::ActionReply::AuthorizationDeniedError) {
            message = i18n("Authorization denied. The system update has not been started.");
        }
        finishTransactionWithError(message);
    });

    QObject::connect(m_job, &KAuth::ExecuteJob::percentChanged, this, [this](KJob *job, unsigned long percent) {
        Q_UNUSED(job)
        if (percent >= 50) {
            setStatus(CommittingStatus);
        }
        setProgress(static_cast<int>(percent));
    });

    setProgress(0);
    setStatus(DownloadingStatus);
    m_job->start();
}

void VanillaVSOTransaction::cancel()
{
    // Cancelling would leave the system in an undefined state, so this is only
    // reachable through programmatic misuse. Report it instead of doing damage.
    qCWarning(VANILLAVSO_LOG) << "Cancelling a Vanilla OS system update is not supported";
    Q_EMIT passiveMessage(i18n("A Vanilla OS system update cannot be cancelled once it has started."));
    setStatus(CancelledStatus);
    deleteLater();
}

void VanillaVSOTransaction::finishTransactionOK()
{
    m_resource->setState(AbstractResource::Installed);
    setProgress(100);
    setStatus(DoneStatus);
    Q_EMIT needReboot();
    deleteLater();
}

void VanillaVSOTransaction::finishTransactionWithError(const QString &message)
{
    qCWarning(VANILLAVSO_LOG) << "Vanilla OS system update failed:" << message;
    Q_EMIT passiveMessage(i18n("The Vanilla OS system update failed") + QStringLiteral(":\n") + message);
    setStatus(DoneWithErrorStatus);
    deleteLater();
}

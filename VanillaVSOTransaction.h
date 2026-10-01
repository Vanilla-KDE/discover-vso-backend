/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KAuth/ExecuteJob>
#include <Transaction/Transaction.h>

class VanillaVSOResource;

/**
 * A single Vanilla OS system image update.
 *
 * The transaction is not cancellable: once `vso upgrade --now` started an
 * image update it has to run to completion. The system needs to be rebooted
 * afterwards to boot into the new image, which is signalled through
 * @c needReboot().
 */
class VanillaVSOTransaction : public Transaction
{
    Q_OBJECT
public:
    VanillaVSOTransaction(VanillaVSOResource *resource, Transaction::Role role);

    void cancel() override;

Q_SIGNALS:
    /**
     * Emitted once the new system image has been written and a reboot is
     * required to complete the update.
     */
    void needReboot();

private:
    void start();
    void finishTransactionOK();
    void finishTransactionWithError(const QString &message);

    VanillaVSOResource *m_resource;
    KAuth::ExecuteJob *m_job = nullptr;
};

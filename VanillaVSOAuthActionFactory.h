/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KAuth/ExecuteJob>

/**
 * Creates the privileged operations that the Vanilla OS backend needs.
 *
 * The actual work is done by @c VanillaVSOKAuthHelper, which runs as root with
 * the helper id "org.kde.discover.vanillavsobackend".
 */
namespace VanillaVSOActionFactory
{
/**
 * Runs @c "vso upgrade check --json" and returns its stdout in the
 * "output" field of the reply data. This action is configured as not requiring
 * authentication.
 */
KAuth::ExecuteJob *createCheckAction();

/**
 * Runs @c "vso upgrade --now" with administrative privileges. This is
 * the action that actually applies a new system image.
 */
KAuth::ExecuteJob *createUpgradeAction();
}

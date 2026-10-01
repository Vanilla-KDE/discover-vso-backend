/*
 *   SPDX-FileCopyrightText: 2024 Mateus Melchiades
 *   SPDX-FileCopyrightText: 2026 KDE Contributors
 *
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "VanillaVSOAuthActionFactory.h"

#include "libdiscover_vanillavso_debug.h"

#include <KAuth/Action>
#include <KLocalizedString>

namespace
{
QString helperId()
{
    return QStringLiteral("org.kde.discover.vanillavsobackend");
}

KAuth::Action createAction(const QString &actionName, const QString &details, int timeout)
{
    KAuth::Action action(helperId() + QLatin1Char('.') + actionName);
    action.setHelperId(helperId());
    if (!action.isValid()) {
        qCWarning(VANILLAVSO_LOG) << "The KAuth action" << action.name() << "is not valid. Is the backend installed correctly?";
        return action;
    }

    action.setDetailsV2(KAuth::Action::DetailsMap{{KAuth::Action::AuthDetail::DetailMessage, details}});
    action.setTimeout(timeout);
    return action;
}
} // namespace

KAuth::ExecuteJob *VanillaVSOActionFactory::createCheckAction()
{
    KAuth::Action action = createAction(QStringLiteral("check"), i18n("Check for Vanilla OS system updates"), 5 * 60 * 1000);
    if (!action.isValid()) {
        return nullptr;
    }
    return action.execute();
}

KAuth::ExecuteJob *VanillaVSOActionFactory::createUpgradeAction()
{
    KAuth::Action action = createAction(QStringLiteral("upgrade"), i18n("Upgrade the Vanilla OS system"), 3 * 60 * 60 * 1000);
    if (!action.isValid()) {
        return nullptr;
    }
    return action.execute();
}

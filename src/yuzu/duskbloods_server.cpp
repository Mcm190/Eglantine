// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include "duskbloods_server.h"

DuskbloodsServer::~DuskbloodsServer() {
    if (process.state() == QProcess::NotRunning) {
        return;
    }
    process.closeWriteChannel();
    if (!process.waitForFinished(5000)) {
        process.terminate();
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished(1000);
        }
    }
}

void DuskbloodsServer::Start() {
    const auto directory = QCoreApplication::applicationDirPath();
    QFile config_file{directory + QStringLiteral("/duskbloods_server.json")};
    // Only builds with a local server configuration opt in. Explicit launchers take precedence.
    if (!config_file.exists() || !qEnvironmentVariableIsEmpty("EDEN_NX2_ONLINE_SERVER")) {
        return;
    }
    const auto fail = [this](const QString& detail) {
        process.closeWriteChannel();
        if (process.state() != QProcess::NotRunning && !process.waitForFinished(5000)) {
            process.terminate();
            process.waitForFinished(5000);
        }
        QMessageBox::warning(nullptr, QObject::tr("Duskbloods local server"),
                             QObject::tr("Could not start the local Duskbloods servers.\n%1")
                                 .arg(detail));
    };
    if (!config_file.open(QIODevice::ReadOnly)) {
        fail(config_file.errorString());
        return;
    }
    const auto config = QJsonDocument::fromJson(config_file.readAll()).object();
    const auto python = config.value(QStringLiteral("python")).toString();
    const auto helper = directory + QStringLiteral("/duskbloods_server.py");
    if (!QFileInfo{python}.isExecutable() || !QFileInfo::exists(helper)) {
        fail(QObject::tr("The server helper or Python environment is missing."));
        return;
    }
    process.start(python, {helper, config_file.fileName()});
    if (!process.waitForStarted(5000)) {
        fail(process.errorString());
        return;
    }

    QElapsedTimer timer;
    timer.start();
    while (!process.canReadLine() && timer.elapsed() < 25000) {
        if (!process.waitForReadyRead(static_cast<int>(25000 - timer.elapsed()))) {
            break;
        }
    }
    const auto env = QJsonDocument::fromJson(process.readLine()).object();
    if (env.value(QStringLiteral("EDEN_NX2_ONLINE_SERVER")).toString().isEmpty() ||
        env.value(QStringLiteral("EDEN_NX2_ONLINE_CA")).toString().isEmpty()) {
        fail(QString::fromUtf8(process.readAllStandardError()));
        return;
    }
    for (auto entry = env.begin(); entry != env.end(); ++entry) {
        const auto name = entry.key().toUtf8();
        if (name.startsWith("EDEN_NX2_") && entry.value().isString() &&
            !qEnvironmentVariableIsSet(name.constData())) {
            qputenv(name.constData(), entry.value().toString().toUtf8());
        }
    }
}

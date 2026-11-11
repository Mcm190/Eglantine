// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QProcess>

class DuskbloodsServer {
public:
    ~DuskbloodsServer();
    void Start();

private:
    QProcess process;
};

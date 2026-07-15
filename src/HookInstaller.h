#pragma once

#include <QString>

class HookInstaller
{
public:
    static QString codexDir();
    static QString hooksPath();
    static QString configPath();

    static bool install(QString *errorMessage);
    static bool uninstall(QString *errorMessage);
};

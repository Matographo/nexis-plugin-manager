#ifndef NEXIS_PLUGINS_H
#define NEXIS_PLUGINS_H

#include <string>
#include <filesystem>
#include <fstream>
#include "Downloader.h"
#include "sqlite3.h"
#include "PackageManager.h"

/**
 * Plugin manager for Downloading and Installing plugins
 */
class PluginManager : public PackageManager {
private:
    struct Package {
        std::string name;
        std::string version;
        bool isHash;
    };

    sqlite3 *db;
    sqlite3_stmt *stmt;

    int createNewVersion(std::string version, bool isHash, std::string packageName, std::string pathToPackage, std::string pathToPackageVersion);
    Package getPackage(std::string packageName);
    
public:
    PluginManager();
    ~PluginManager();

    int install(std::string package) override;
    int install(std::vector<std::string> packages) override;
    int uninstall(std::string package) override;
    int uninstall(std::vector<std::string> packages) override;
    int update(std::string package) override;
    int update(std::vector<std::string> packages) override;
    int update() override;
    int search(std::string package) override;
    int list() override;
    int info(std::string package) override;
    
};

#endif // NEXIS_PLUGINS_H
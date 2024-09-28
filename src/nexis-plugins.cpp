#include "nexis-plugins.h"

PluginManager::PluginManager() {
    sqlite3 *db;
    
    std::string homePath = std::getenv("HOME");
    std::string pathToDatabase = homePath + "/.nxpm/databases/nexis-plugins";
    std::string path = pathToDatabase;
    if(!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
    path += "/cpp.db";
    if(!std::filesystem::exists(path)) {
        Downloader downloader;
        downloader.downloadGit("Matographo/nexis-plugins-database", pathToDatabase);
    }
    
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins";
    if(!std::filesystem::exists(pathToPackage)) {
        std::filesystem::create_directories(pathToPackage);
    }
    
    this->db = db;
    sqlite3_open(path.c_str(), &this->db);
}

PluginManager::~PluginManager() {
    sqlite3_close(this->db);
}

int PluginManager::install(std::string package) {
    PluginManager::Package p = getPackage(package);

    std::string packageName = p.name;
    std::string packageVersion = p.version;

    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins";
    std::string pathToPackageVersion = pathToPackage + "/" + package + "/" + packageVersion;
    
    if(std::filesystem::exists(pathToPackageVersion + "/include") && std::filesystem::exists(pathToPackageVersion + "/src") 
        && std::filesystem::is_directory(pathToPackageVersion + "/include") && std::filesystem::is_directory(pathToPackageVersion + "/src")) {
        return 0;
    }
    
    if(!std::filesystem::exists(pathToPackage)) {
        std::filesystem::create_directories(pathToPackage);
    }
    
    if(!std::filesystem::exists(pathToPackage + "/.raw")) {
        std::filesystem::create_directories(pathToPackage + "/.raw");
    }
    
    std::string sqlSelect = "SELECT repo FROM plugins WHERE name = ?";
    
    int sqlResult = sqlite3_prepare_v2(
        this->db,
        sqlSelect.c_str(),
        -1,
        &this->stmt,
        NULL
    );
    
    if(sqlResult != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement" << std::endl;
        return 1;
    }
    
    sqlite3_bind_text(this->stmt, 1, packageName.c_str(), -1, SQLITE_STATIC);
    
    if(sqlite3_step(this->stmt) != SQLITE_ROW) {
        std::cerr << "Package not found with name " << packageName << std::endl;
        return 1;
    }     
    
    std::string repo = (char*)sqlite3_column_text(this->stmt, 0);
    
    if(repo == "") {
        std::cerr << "Package not found with name " << packageName << std::endl;
        return 1;
    }
    
    Downloader downloader;
    if(downloader.downloadGit(repo, pathToPackage + "/.raw") != 0) {
        std::cerr << "Failed to download package " << packageName << std::endl;
        return 1;
    }

    return this->createNewVersion(packageVersion, p.isHash, packageName, pathToPackage, pathToPackageVersion);
}


PluginManager::Package PluginManager::getPackage(std::string packageName) {
    PluginManager::Package package;
    if(packageName.find("@") != std::string::npos) {
        package.version = packageName.substr(packageName.find("@") + 1);
        package.name    = packageName.substr(0, packageName.find("@"));
        package.isHash  = false;
        return package;
    } else if(packageName.find("#") != std::string::npos) {
        package.version = packageName.substr(packageName.find("#") + 1);
        package.name    = packageName.substr(0, packageName.find("#"));
        package.isHash  = true;
        return package;
    }
    return package;
}


int PluginManager::createNewVersion(std::string version, bool isHash, std::string packageName, std::string pathToPackage, std::string pathToPackageVersion) {
    std::filesystem::create_directories(pathToPackageVersion);
    
    std::string gitCommand = "git -C " + pathToPackage + "/.raw ";

    if(isHash) {
        gitCommand += "checkout " + version;
    } else {
        gitCommand += "checkout tags/" + version;
    }
    
    if(system(gitCommand.c_str()) != 0) {
        std::cerr << "Failed to checkout version " << version << " for package " << packageName << std::endl;
        std::filesystem::remove_all(pathToPackageVersion);
        return 1;
    }

    std::string pathToRepo = pathToPackage + "/.raw";
    std::string pathToBuild = pathToPackage + "/build";
    
    std::filesystem::create_directories(pathToPackageVersion + "/include");
    std::filesystem::create_directories(pathToPackageVersion + "/cpp");
    std::filesystem::create_directories(pathToBuild);

    if(std::filesystem::exists(pathToRepo + "/include")) {
        std::filesystem::copy(pathToRepo + "/include", pathToPackageVersion + "/include", std::filesystem::copy_options::recursive);
    } else {
        for(const auto & entry : std::filesystem::directory_iterator(pathToRepo)) {
            if(entry.path().extension() == ".h" || entry.path().extension() == ".hpp") {
                std::filesystem::copy(entry.path(), pathToPackageVersion + "/include");
            }
        }
    }
    
    std::string cmakeBuild = "cmake -S " + pathToRepo + " -B " + pathToBuild;
    
    if(system(cmakeBuild.c_str()) != 0) {
        std::cerr << "Failed to generate build files for package " << packageName << std::endl;
        std::filesystem::remove_all(pathToPackageVersion);
        std::filesystem::remove_all(pathToBuild);
        return 1;
    }
    
    std::string makeBuild = "make -C " + pathToBuild;
    
    if(system(makeBuild.c_str()) != 0) {
        std::cerr << "Failed to build package " << packageName << std::endl;
        std::filesystem::remove_all(pathToPackageVersion);
        std::filesystem::remove_all(pathToBuild);
        return 1;
    }
    
    std::string libraryFile;
    std::string libraryExtention;

    for(const auto & entry : std::filesystem::directory_iterator(pathToBuild)) {
        libraryExtention = entry.path().extension();
        if(libraryExtention == ".so" || libraryExtention == ".a" || libraryExtention == ".dll" || libraryExtention == ".lib") {
            libraryFile = entry.path();
            break;
        }
    }
    if(libraryFile == "") {
        std::cerr << "No library file found for package " << packageName << std::endl;
        std::filesystem::remove_all(pathToPackageVersion);
        std::filesystem::remove_all(pathToBuild);
        return 1;
    }
    
    std::filesystem::copy(libraryFile, pathToPackageVersion + "/cpp/" + packageName + libraryExtention);
    
    std::filesystem::remove_all(pathToBuild);
    return 0;
}

int PluginManager::install(std::vector<std::string> packages) {
    for(std::string package : packages) {
        if(install(package) != 0) {
            return 1;
        }
    }
    return 0;
}

int PluginManager::uninstall(std::string package) {
    PluginManager::Package p = getPackage(package);

    std::string packageName = p.name;
    std::string packageVersion = p.version;

    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins/" + packageName;
    std::string pathToPackageVersion = pathToPackage + "/" + packageVersion;
    
    std::filesystem::remove_all(pathToPackageVersion);
    
    bool found = false;
    
    for(const auto & entry : std::filesystem::directory_iterator(pathToPackage)) {
        if(entry.path().filename() != ".raw") {
            found = true;
            break;
        }
    }
    if(!found) {
        std::filesystem::remove_all(pathToPackage);
    }

    return 0;
}

int PluginManager::uninstall(std::vector<std::string> packages) {
    for(std::string package : packages) {
        if(uninstall(package) != 0) {
            return 1;
        }
    }
    return 0;
}

int PluginManager::update(std::string package) {
    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins/" + package;
    if(!std::filesystem::exists(pathToPackage + "/.raw")) {
        std::cerr << "No package found with name " << package << std::endl;
        return 1;
    }

    std::string gitCommand = "git -C " + pathToPackage + "/.raw pull";
    int result = system(gitCommand.c_str());
    if(result != 0) {
        std::cerr << "Failed to update package " << package << std::endl;
        return 1;
    }
    return 0;
}

int PluginManager::update(std::vector<std::string> packages) {
    for(std::string package : packages) {
        if(update(package) != 0) {
            return 1;
        }
    }
    return 0;
}

int PluginManager::update() {
    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins";
    std::vector<std::string> packages;
    for(const auto & entry : std::filesystem::directory_iterator(pathToPackage)) {
        packages.push_back(entry.path().stem());
    }
    return this->update(packages);
}

int PluginManager::search(std::string package) {
    std::string sqlSelect = "SELECT * FROM plugins WHERE name LIKE ?";
    
    int sqlResult = sqlite3_prepare_v2(
        this->db,
        sqlSelect.c_str(),
        -1,
        &this->stmt,
        NULL
    );
    
    if(sqlResult != SQLITE_OK) {
        std::cerr << "Failed to prepare SQL statement" << std::endl;
        return 1;
    }
    
    sqlite3_bind_text(this->stmt, 1, package.c_str(), -1, SQLITE_STATIC);
    
    bool found = false;
    
    while(sqlite3_step(this->stmt) == SQLITE_ROW) {
        std::cout << "Package " << (char *)sqlite3_column_text(this->stmt, 0) << " found at " << (char *)sqlite3_column_text(this->stmt, 1) << std::endl;
        found = true;
    }
    
    sqlite3_finalize(this->stmt);

    if(!found) {
        std::cerr << "No package found with name " << package << std::endl;
        return 1;
    }
    
    return 0;
}

int PluginManager::list() {
    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins";

    for(const auto & entry : std::filesystem::directory_iterator(pathToPackage)) {
        std::cout << entry.path().stem() << std::endl;
    }
    
    return 0;
}

int PluginManager::info(std::string package) {
    std::string sqlSelect = "SELECT * FROM plugins WHERE name = ?";

    int sqlResult = sqlite3_prepare_v2(
        this->db,
        sqlSelect.c_str(),
        -1,
        &this->stmt,
        NULL
    );

    sqlite3_bind_text(this->stmt, 1, package.c_str(), -1, SQLITE_STATIC);
    
    sqlite3_step(this->stmt);

    std::string repo = (char*)sqlite3_column_text(this->stmt, 1);

    sqlite3_finalize(this->stmt);

    if(repo == "") {
        std::cerr << "No package found with name " << package << std::endl;
        return 1;
    }

    std::string homePath = std::getenv("HOME");
    std::string pathToPackage = homePath + "/.nxpm/plugins/nexis-plugins/" + package;
    
    if(!std::filesystem::exists(pathToPackage)) {
        std::cerr << "No package " << package << " is installed" << std::endl;
    }
    
    if(!std::filesystem::exists(pathToPackage + "/.raw")) {
        std::cerr << "No package found with name " << package << std::endl;
        return 1;
    }
    
    std::string gitCommand = "git -C " + pathToPackage + "/.raw checkout HEAD";
    
    if(system(gitCommand.c_str()) != 0) {
        std::cerr << "Failed to checkout HEAD for package " << package << std::endl;
        return 1;
    }
    
    std::stringstream output;

    output << "Package: " << package << std::endl;
    output << "Repository: " << repo << std::endl;

    if(std::filesystem::exists(pathToPackage + "/.raw/README.md")) {
        output << "README:" << std::endl;
        std::ifstream readmeFile(pathToPackage + "/.raw/README.md");
        output << readmeFile.rdbuf();
        readmeFile.close();
    }
    
    std::cout << output.str();

    return 0;
}


extern "C" PackageManager * create() {
    return new PluginManager();
}

extern "C" void destroy(PackageManager * manager) {
    delete manager;
}
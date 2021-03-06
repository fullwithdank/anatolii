#pragma once
#include <filesystem>

#include "SDK\Color.h"

/* forward declarations so i wont have to include these */
class MenuMain;

namespace fs = std::experimental::filesystem;

class Settings
{
public:
    void Initialize(MenuMain* pMenuObj);

    void SaveSettings(const std::string& strFileName, MenuMain* pMenuObj);
    void LoadSettings(const std::string& strFileName, MenuMain* pMenuObj);

private:
    void UpdateDirectoryContent(const fs::path& fsPath);
    inline fs::path GetFolderPath();

    fs::path                 fsPath{};
    std::vector<std::string> vecFileNames{};

public:
    /* All our settings variables will be here  *
    * The best would be if they'd get          *
    * initialized in the class itself.         */

    bool  bCheatActive = true;
    bool  bMenuOpened  = false;
    bool  bBhopEnabled = true;
    bool  bShowBoxes   = false;
    bool  bShowNames   = true;
    bool  bShowWeapons = false;
	int   nCustomColors= 0;
	int   nFov         = 90; //def fov value 
	int   nViewmodelFov= 0;  //mayb i can use same method :o

    Color colCursor = Color(0, 150, 255, 100);
};

extern Settings g_Settings;


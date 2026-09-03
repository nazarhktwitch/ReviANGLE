#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")

namespace fs = std::filesystem;

constexpr const char* DLL_FILES[] = {
    "opengl32.dll",
    "libEGL.dll",
    "libGLESv2.dll",
    "d3dcompiler_47.dll",
    "vulkan-1.dll"
};

constexpr const char* CONFIG_FILES[] = {
    "angle_config.ini",
    "gd-angle-editor.exe",
    "angle_log.txt",
    "gdangle_loaded.txt"
};

struct UninstallResult {
    bool success = true;
    std::string message;
    std::vector<std::string> deleted_files;
    std::vector<std::string> failed_files;
};

// Get Steam path from registry
std::string GetSteamPath() {
    HKEY hKey;
    char steamPath[MAX_PATH] = "";
    DWORD size = sizeof(steamPath);

    // Try HKEY_CURRENT_USER first
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SteamPath", NULL, NULL, (LPBYTE)steamPath, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(steamPath);
        }
        RegCloseKey(hKey);
    }

    // Try HKEY_LOCAL_MACHINE for system-wide installation
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (LPBYTE)steamPath, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::string(steamPath);
        }
        RegCloseKey(hKey);
    }

    return "";
}

// Find Geometry Dash path
std::string FindGeometryDash() {
    std::vector<std::string> paths_to_check;

    // Try registry Steam path first
    std::string steam_path = GetSteamPath();
    if (!steam_path.empty()) {
        // Normalize path separators
        std::replace(steam_path.begin(), steam_path.end(), '/', '\\');
        paths_to_check.push_back(steam_path + "\\steamapps\\common\\Geometry Dash");
    }

    // Add common default paths
    paths_to_check.push_back("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Geometry Dash");
    paths_to_check.push_back("C:\\Program Files\\Steam\\steamapps\\common\\Geometry Dash");

    // Add custom Steam library paths
    paths_to_check.push_back("D:\\Games\\SteamGames\\steamapps\\common\\Geometry Dash");
    paths_to_check.push_back("E:\\SteamLibrary\\steamapps\\common\\Geometry Dash");

    for (const auto& path : paths_to_check) {
        if (fs::exists(path + "\\GeometryDash.exe")) {
            return path;
        }
    }

    return "";
}

UninstallResult UninstallReviANGLE(const std::string& gd_path) {
    UninstallResult result;

    if (gd_path.empty()) {
        result.success = false;
        result.message = "Invalid Geometry Dash path provided.";
        return result;
    }

    // Check if GeometryDash.exe exists
    std::string gd_exe = gd_path + "\\GeometryDash.exe";
    if (!fs::exists(gd_exe)) {
        result.success = false;
        result.message = "GeometryDash.exe not found at:\n" + gd_path;
        return result;
    }

    // Delete DLL files
    for (const auto& dll : DLL_FILES) {
        std::string file_path = gd_path + "\\" + dll;
        if (fs::exists(file_path)) {
            try {
                fs::remove(file_path);
                result.deleted_files.push_back(dll);
            } catch (const std::exception& e) {
                result.failed_files.push_back(std::string(dll) + " (error: " + e.what() + ")");
            }
        }
    }

    // Delete config and helper files
    for (const auto& config : CONFIG_FILES) {
        std::string file_path = gd_path + "\\" + config;
        if (fs::exists(file_path)) {
            try {
                fs::remove(file_path);
                result.deleted_files.push_back(config);
            } catch (const std::exception& e) {
                result.failed_files.push_back(std::string(config) + " (error: " + e.what() + ")");
            }
        }
    }

    // Delete cache folders
    const char* cache_folders[] = { "shader_cache", "plist_cache" };
    for (const auto& folder : cache_folders) {
        std::string folder_path = gd_path + "\\" + folder;
        if (fs::exists(folder_path)) {
            try {
                fs::remove_all(folder_path);
                result.deleted_files.push_back(std::string(folder) + "/ (directory)");
            } catch (const std::exception& e) {
                result.failed_files.push_back(std::string(folder) + "/ (error: " + e.what() + ")");
            }
        }
    }
	
    // Delete repo files
    const char* repo_files[] = { "README.md", "LICENSE" };
    for (const auto& file : repo_files) {
        std::string file_path = gd_path + "\\" + file;
        if (fs::exists(file_path)) {
            try {
                fs::remove(file_path);
                result.deleted_files.push_back(file);
            } catch (const std::exception& e) {
                result.failed_files.push_back(std::string(file) + " (error: " + e.what() + ")");
            }
        }
    }

    if (!result.failed_files.empty()) {
        result.success = false;
    }

    return result;
}

void SelectGDPath(std::string& out_path) {
    OPENFILENAMEA ofn = {};
    char file_path[MAX_PATH] = "";
    
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFilter = "Geometry Dash (GeometryDash.exe)\0GeometryDash.exe\0Executable Files (*.exe)\0*.exe\0";
    ofn.lpstrFile = file_path;
    ofn.nMaxFile = sizeof(file_path);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Select GeometryDash.exe";

    if (GetOpenFileNameA(&ofn)) {
        out_path = file_path;
        // Extract directory path
        size_t pos = out_path.rfind("\\");
        if (pos != std::string::npos) {
            out_path = out_path.substr(0, pos);
        }
    }
}

void DisplayResults(HWND hwnd, const UninstallResult& result) {
    std::string main_instruction = result.success ? 
        "ReviANGLE has been successfully uninstalled." : 
        "Uninstallation completed with some warnings.";
    
    std::string content = "Deleted files:\n";
    for (const auto& file : result.deleted_files) {
        content += "  - " + file + "\n";
    }

    if (!result.failed_files.empty()) {
        content += "\nFailed to remove:\n";
        for (const auto& file : result.failed_files) {
            content += "  - " + file + "\n";
        }
    }

    std::string full_msg = main_instruction + "\n\n" + content;
    MessageBoxA(hwnd, full_msg.c_str(), "ReviANGLE Uninstaller", 
        result.success ? MB_ICONINFORMATION : MB_ICONWARNING);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Find Geometry Dash automatically
    std::string gd_path = FindGeometryDash();

    if (gd_path.empty()) {
        // Not found - ask user to locate manually
        int result = MessageBoxA(NULL, 
            "Geometry Dash not found.\n\nWould you like to select it manually?",
            "ReviANGLE Uninstaller", MB_YESNO | MB_ICONQUESTION);

        if (result != IDYES) {
            return 0;
        }

        SelectGDPath(gd_path);
        if (gd_path.empty()) {
            MessageBoxA(NULL, "Uninstallation cancelled.", "ReviANGLE Uninstaller", MB_ICONINFORMATION);
            return 0;
        }

        // Verify the selected path
        if (!fs::exists(gd_path + "\\GeometryDash.exe")) {
            MessageBoxA(NULL, "GeometryDash.exe not found in the selected folder.", 
                "ReviANGLE Uninstaller", MB_ICONERROR);
            return 1;
        }
    }

    // Ask for confirmation with detected/selected path
    std::string confirm_msg = "Detected Geometry Dash at:\n\n" + gd_path + 
        "\n\nContinue with uninstallation?";
    int result = MessageBoxA(NULL, confirm_msg.c_str(), "ReviANGLE Uninstaller", 
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (result != IDYES) {
        return 0;
    }

    // Perform uninstallation
    UninstallResult uninstall_result = UninstallReviANGLE(gd_path);
    DisplayResults(NULL, uninstall_result);

    if (uninstall_result.success) {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(NULL, exePath, MAX_PATH) > 0) {
            wchar_t cmd[MAX_PATH * 2];
            swprintf_s(cmd, L"cmd.exe /c timeout /t 1 /nobreak > NUL & del /f /q \"%s\"", exePath);

            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi = {};
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
        }
    }

    return uninstall_result.success ? 0 : 1;
}

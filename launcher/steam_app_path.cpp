#include "steam_app_path.h"

// because of vdf_parser.hpp
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING true
#define UNICODE

#include <iostream>
#include <optional>
#include <filesystem>
#include <windows.h>
#include <winreg/WinReg.hpp>
#include <vdf_parser.hpp>

namespace fs = std::filesystem;

auto parse_vdf(const fs::path path)
{
    auto file = std::ifstream { path };
    return tyti::vdf::read(file);
}

//TODO: std::nullopt doesn't work for some reason
auto nullpath() noexcept
{
    return std::optional<fs::path> {};
}

auto maybe_path(const fs::path path) -> std::optional<fs::path>
{
    if (!fs::exists(path))
        return std::nullopt;
    return std::make_optional(path.lexically_normal());
}

auto get_steam_path()
{
    auto key = winreg::RegKey { HKEY_CURRENT_USER, LR"(Software\Valve\Steam)" };
    return fs::path { key.GetStringValue(L"SteamPath") };
}

auto get_steam_libraries()
{
    auto steam_path = get_steam_path();
    auto libraries = std::vector<fs::path> { steam_path };
    auto root = parse_vdf(steam_path / "SteamApps" / "libraryfolders.vdf");
    for (size_t i = 0; i < root.childs.size(); i++) {
        libraries.emplace_back(root.childs.at(std::to_string(i))->attribs[std::string("path")]);
    }
    return libraries;
}

auto get_manifest_path(std::wstring_view app_id)
{
    using namespace std::string_literals;

    const auto filename = L"appmanifest_"s.append(app_id).append(L".acf");
    for (const auto& library : get_steam_libraries()) {
        auto manifest_path = maybe_path(library / "steamapps" / filename);
        if (manifest_path)
            return manifest_path;
    }

    return nullpath();
}

auto check_custom_game_path(const fs::path& game_exe) -> std::optional<fs::path>
{
    const auto custom_path = fs::path(L"E:\\Elden Ring\\Game");
    if (fs::exists(custom_path / game_exe)) {
        return maybe_path(custom_path);
    }
    return nullpath();
}

std::optional<fs::path> get_game_path(std::wstring_view app_id)
{
    // First check custom path for Elden Ring
    if (app_id == L"1245620") {  // Elden Ring's app_id
        auto custom_path = check_custom_game_path(L"eldenring.exe");
        if (custom_path) {
            return custom_path;
        }
    }

    // Fall back to Steam path if custom path fails
    const auto manifest_path = get_manifest_path(app_id);
    if (!manifest_path)
        return nullpath();

    const auto manifest = parse_vdf(manifest_path.value());
    return maybe_path(manifest_path->parent_path() / "common" / manifest.attribs.at("installdir"));
}
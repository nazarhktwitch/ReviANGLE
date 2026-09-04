#include "editor_app.hpp"
#include "imgui.h"
#include "ini_parser.hpp"
#include "schema.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

// The Ini lives as a process-global so the EditorApp methods don't have to
// pass it around - this is a small single-window app, no concurrency.
static Ini g_ini;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

static std::string nowHHMMSS() {
  std::time_t t = std::time(nullptr);
  std::tm lt{};
  localtime_s(&lt, &t);
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.tm_hour, lt.tm_min,
                lt.tm_sec);
  return buf;
}

// Status-tag color: parses the first non-bracket char (✓, O for ON, OFF, ...).
static ImVec4 statusColor(const char *status) {
  if (!status || !*status)
    return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
  // Scan for ON / OFF tokens.
  if (std::strncmp(status, "ON", 2) == 0)
    return ImVec4(0.30f, 0.85f, 0.40f, 1.0f);
  if (std::strncmp(status, "OFF", 3) == 0)
    return ImVec4(0.85f, 0.40f, 0.30f, 1.0f);
  return ImVec4(0.85f, 0.75f, 0.30f, 1.0f);
}

static bool parseBoolStr(const std::string &v) {
  std::string s;
  for (char c : v)
    s.push_back((char)std::tolower((unsigned char)c));
  return s == "true" || s == "1" || s == "yes" || s == "on";
}

// Find the schema entry for a given section/key. Returns -1 if not in schema.
static int findOptIdx(const std::string &section, const std::string &key) {
  const auto &all = schemaAll();
  for (int i = 0; i < (int)all.size(); ++i) {
    if (all[i].section == section && all[i].key == key)
      return i;
  }
  return -1;
}

// Filter logic: option appears if section matches the active tab AND
// (search empty OR key/section contains search).
static bool optionVisible(const OptionDef &o, const std::string &sectionFilter,
                          const std::string &search) {
  if (o.section != sectionFilter)
    return false;
  if (search.empty())
    return true;
  auto contains = [&](const char *s) {
    std::string l;
    for (const char *p = s; *p; ++p)
      l.push_back((char)std::tolower((unsigned char)*p));
    std::string q;
    for (char c : search)
      q.push_back((char)std::tolower((unsigned char)c));
    return l.find(q) != std::string::npos;
  };
  return contains(o.key) || contains(o.section);
}

// ----------------------------------------------------------------------------
// EditorApp
// ----------------------------------------------------------------------------

bool EditorApp::init(const std::string &iniPath) {
  m_iniPath = iniPath;
  if (!g_ini.load(iniPath)) {
    // Empty INI is OK - user can save fresh defaults.
    // We still pre-populate with defaults from schema so toggles aren't blank.
    for (const auto &o : schemaAll()) {
      if (!g_ini.has(o.section, o.key))
        g_ini.set(o.section, o.key, o.default_value);
    }
  }
  return true;
}

void EditorApp::doSave() {
  g_ini.save(m_iniPath);
  m_dirty = false;
  m_savedAt = "saved " + nowHHMMSS();
}

void EditorApp::doReload() {
  g_ini = Ini{};
  g_ini.load(m_iniPath);
  m_dirty = false;
  m_savedAt = "reloaded " + nowHHMMSS();
}

void EditorApp::doResetDefaults() {
  for (const auto &o : schemaAll()) {
    g_ini.set(o.section, o.key, o.default_value);
  }
  m_dirty = true;
  m_savedAt = "defaults loaded - press Save to persist";
}

// ----------------------------------------------------------------------------
// Render
// ----------------------------------------------------------------------------

void EditorApp::renderFrame() {
  ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->WorkPos);
  ImGui::SetNextWindowSize(vp->WorkSize);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

  ImGuiWindowFlags wflags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::Begin("##root", nullptr, wflags);
  ImGui::PopStyleVar(3);

  renderTopBar();
  renderSectionTabs();

  // Content area split: left ~55%, right ~45%.
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float footerH = ImGui::GetFrameHeightWithSpacing() + 12.0f;
  ImVec2 contentSize(avail.x, avail.y - footerH);

  float leftW = contentSize.x * 0.55f;
  ImGui::BeginChild("##left", ImVec2(leftW, contentSize.y), false);
  renderOptionList();
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("##right", ImVec2(0, contentSize.y), false);
  renderDescriptionPanel();
  ImGui::EndChild();

  renderFooter();
  renderConfirmModal();

  ImGui::End();
}

void EditorApp::renderTopBar() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
  ImGui::BeginChild("##topbar", ImVec2(0, 36), false);

  float topY = (36.0f - ImGui::GetFrameHeight()) * 0.5f;
  ImGui::SetCursorPosY(topY);
  ImGui::SetCursorPosX(10.0f);

  char buf[128];
  std::strncpy(buf, m_search.c_str(), sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  float searchW = 300.0f;
  ImGui::SetNextItemWidth(searchW);
  if (ImGui::InputTextWithHint("##search", "Search options...", buf, sizeof(buf))) {
    m_search = buf;
  }

  if (!m_search.empty()) {
    ImGui::SameLine();
    ImGui::SetCursorPosY(topY);
    if (ImGui::Button("Clear")) {
      m_search.clear();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void EditorApp::renderSectionTabs() {
  const auto &secs = schemaSections();
  if (ImGui::BeginTabBar("##sections", ImGuiTabBarFlags_FittingPolicyScroll |
                                           ImGuiTabBarFlags_Reorderable)) {
    for (int i = 0; i < (int)secs.size(); ++i) {
      // Count visible options in this section (for badge).
      int visibleCount = 0;
      for (const auto &o : schemaAll()) {
        if (optionVisible(o, secs[i], m_search))
          ++visibleCount;
      }
      char label[96];
      std::snprintf(label, sizeof(label), "%s (%d)###tab_%d", secs[i].c_str(),
                    visibleCount, i);
      ImGuiTabItemFlags flags = 0;
      if (m_currentSectionIdx == i) {
        // We don't force selection; ImGui handles it via user clicks.
      }
      if (ImGui::BeginTabItem(label, nullptr, flags)) {
        m_currentSectionIdx = i;
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
}

void EditorApp::renderOptionList() {
  const auto &secs = schemaSections();
  if (m_currentSectionIdx < 0 || m_currentSectionIdx >= (int)secs.size())
    return;
  const std::string &sec = secs[m_currentSectionIdx];

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

  const auto &all = schemaAll();
  for (int i = 0; i < (int)all.size(); ++i) {
    const auto &o = all[i];
    if (!optionVisible(o, sec, m_search))
      continue;

    ImGui::PushID(i);

    // Status colored dot.
    ImVec4 col = statusColor(o.status);
    ImGui::TextColored(col, "●");
    ImGui::SameLine();

    // Control by type.
    const std::string current = g_ini.get(o.section, o.key);
    switch (o.type) {
    case OptType::Bool: {
      bool v = parseBoolStr(current);
      if (ImGui::Checkbox(o.key, &v)) {
        g_ini.set(o.section, o.key, v ? "true" : "false");
        m_dirty = true;
      }
      break;
    }
    case OptType::Int: {
      int v = std::atoi(current.c_str());
      ImGui::SetNextItemWidth(120);
      if (ImGui::InputInt(o.key, &v, 1, 10)) {
        if (v < o.min_int)
          v = o.min_int;
        if (v > o.max_int)
          v = o.max_int;
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%d", v);
        g_ini.set(o.section, o.key, buf);
        m_dirty = true;
      }
      break;
    }
    case OptType::String:
    case OptType::Hex: {
      char buf[256];
      std::strncpy(buf, current.c_str(), sizeof(buf) - 1);
      buf[sizeof(buf) - 1] = '\0';
      ImGui::SetNextItemWidth(200);
      if (ImGui::InputText(o.key, buf, sizeof(buf))) {
        g_ini.set(o.section, o.key, buf);
        m_dirty = true;
      }
      break;
    }
    case OptType::Enum: {
      // Split o.enum_values on commas into a static-ish list, then
      // render an ImGui::Combo. Each option is a short string so
      // splitting per-frame is cheap (n is typically 2..5).
      std::vector<std::string> choices;
      {
        std::string s = o.enum_values ? o.enum_values : "";
        size_t start = 0;
        while (start <= s.size()) {
          size_t comma = s.find(',', start);
          if (comma == std::string::npos)
            comma = s.size();
          std::string item = s.substr(start, comma - start);
          // trim leading/trailing space.
          while (!item.empty() && std::isspace((unsigned char)item.front()))
            item.erase(item.begin());
          while (!item.empty() && std::isspace((unsigned char)item.back()))
            item.pop_back();
          if (!item.empty())
            choices.push_back(item);
          if (comma == s.size())
            break;
          start = comma + 1;
        }
      }
      int currentIdx = 0;
      for (int j = 0; j < (int)choices.size(); ++j) {
        if (choices[j] == current) {
          currentIdx = j;
          break;
        }
      }
      std::vector<const char *> cstrs;
      cstrs.reserve(choices.size());
      for (auto &c : choices)
        cstrs.push_back(c.c_str());

      ImGui::SetNextItemWidth(200);
      if (ImGui::Combo(o.key, &currentIdx, cstrs.data(), (int)cstrs.size())) {
        g_ini.set(o.section, o.key, choices[currentIdx]);
        m_dirty = true;
      }
      break;
    }
    }

    // Selecting option for the description panel: hover or click highlights.
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
      m_selectedOptIdx = i;
    }

    // Warning badge if setting has risks
    if (o.warning_en && *o.warning_en) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f), "⚠️");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "DANGEROUS / RISK: Check description panel for warnings");
      }
    }

    ImGui::PopID();
  }

  ImGui::PopStyleVar(2);

  // If nothing selected yet (first frame on a new tab), pick the first
  // visible option so the right panel isn't empty.
  if (m_selectedOptIdx < 0 || all[m_selectedOptIdx].section != sec) {
    for (int i = 0; i < (int)all.size(); ++i) {
      if (optionVisible(all[i], sec, m_search)) {
        m_selectedOptIdx = i;
        break;
      }
    }
  }
}

void EditorApp::renderDescriptionPanel() {
  const auto &all = schemaAll();
  if (m_selectedOptIdx < 0 || m_selectedOptIdx >= (int)all.size()) {
    ImGui::TextDisabled("Hover an option to see its description here.");
    return;
  }
  const auto &o = all[m_selectedOptIdx];

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 8));

  // Header: section.key + status badge.
  ImGui::Dummy(ImVec2(0, 4));
  ImGui::TextColored(ImVec4(0.55f, 0.65f, 0.85f, 1.0f), "[%s]", o.section);
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", o.key);

  ImGui::TextColored(statusColor(o.status), "Status:  %s", o.status);

  const char *typeStr = "?";
  switch (o.type) {
  case OptType::Bool:
    typeStr = "bool";
    break;
  case OptType::Int:
    typeStr = "int";
    break;
  case OptType::String:
    typeStr = "string";
    break;
  case OptType::Hex:
    typeStr = "hex";
    break;
  case OptType::Enum:
    typeStr = "enum";
    break;
  }
  ImGui::TextDisabled("Type: %s   Default: %s", typeStr, o.default_value);
  if (o.type == OptType::Int) {
    ImGui::TextDisabled("Range: %d .. %d", o.min_int, o.max_int);
  }
  if (o.type == OptType::Enum && o.enum_values && *o.enum_values) {
    ImGui::TextDisabled("Choices: %s", o.enum_values);
  }

  if (o.warning_en && *o.warning_en) {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f),
                       "RISK WARNING / ПРЕДУПРЕЖДЕНИЕ:");
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.45f, 1.0f), "EN: %s",
                       o.warning_en);
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.45f, 1.0f), "RU: %s",
                       o.warning_ru);
    ImGui::PopTextWrapPos();
  }

  ImGui::Separator();

  ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.95f, 1.0f), "EN");
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(o.desc_en);
  ImGui::PopTextWrapPos();

  ImGui::Dummy(ImVec2(0, 4));
  ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.6f, 1.0f), "RU");
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(o.desc_ru);
  ImGui::PopTextWrapPos();

  ImGui::PopStyleVar();
}

void EditorApp::renderFooter() {
  ImGui::Separator();

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14, 6));

  // Save (highlighted when dirty).
  if (m_dirty) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.25f, 0.70f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.18f, 0.50f, 0.25f, 1.0f));
  }
  if (ImGui::Button("Save  (Ctrl+S)"))
    doSave();
  if (m_dirty)
    ImGui::PopStyleColor(3);

  ImGui::SameLine();
  if (ImGui::Button("Reload from disk"))
    m_pendingConfirm = ConfirmKind::Reload;
  ImGui::SameLine();
  if (ImGui::Button("Reset to defaults"))
    m_pendingConfirm = ConfirmKind::ResetDefaults;

  // Status text on the right.
  ImGui::SameLine();
  if (m_dirty) {
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.30f, 1.0f),
                       "  ●  unsaved changes");
  } else if (!m_savedAt.empty()) {
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "  ✓  %s",
                       m_savedAt.c_str());
  }

  ImGui::PopStyleVar();

  // Ctrl+S keyboard shortcut.
  ImGuiIO &io = ImGui::GetIO();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
    doSave();
}

void EditorApp::renderConfirmModal() {
  if (m_pendingConfirm == ConfirmKind::None)
    return;

  const char *title = (m_pendingConfirm == ConfirmKind::Reload)
                          ? "Reload from disk?###confirm"
                          : "Reset to defaults?###confirm";

  ImGui::OpenPopup(title);
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(title, nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    if (m_pendingConfirm == ConfirmKind::Reload) {
      ImGui::TextWrapped("Discard unsaved changes and reload %s from disk?",
                         m_iniPath.c_str());
    } else {
      ImGui::TextWrapped("Reset every option to its schema-defined default? "
                         "Comments and descriptions are preserved; only "
                         "key=value lines change. You'll still need to "
                         "press Save to persist to disk.");
    }
    ImGui::Dummy(ImVec2(0, 8));
    if (ImGui::Button("Yes", ImVec2(120, 0))) {
      if (m_pendingConfirm == ConfirmKind::Reload)
        doReload();
      else
        doResetDefaults();
      m_pendingConfirm = ConfirmKind::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_pendingConfirm = ConfirmKind::None;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

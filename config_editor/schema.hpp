// Single source of truth for what options exist, what type they are, and the
// bilingual (EN/RU) description shown in the right-hand panel.
//
// The schema is built from the curated angle_config.ini so the GUI shows
// exactly the same wording the user can read in the file. Adding a new option
// here is enough - the UI auto-discovers it from the table.

#pragma once
#include <string>
#include <vector>

enum class OptType { Bool, Int, String, Hex, Enum };

struct OptionDef {
  const char *section; // e.g. "BoostLatency"
  const char *key;     // e.g. "frame_pacing_target"
  OptType type;
  const char *default_value; // string-form default, used by "Reset to defaults"
  const char *desc_en;       // English description (multi-line ok)
  const char *desc_ru;       // Russian description (multi-line ok)
  const char *status;        // "ON" / "OFF - reason" / "tune-me" - short tag
  int min_int = 0;           // only for Int
  int max_int = 65535;
  // For OptType::Enum: comma-separated allowed values, e.g. "d3d11,d3d9".
  // Renders as an ImGui::Combo. Empty for non-enum types.
  const char *enum_values = "";
};

// Returns the static schema. Order matches the grouping in angle_config.ini
// so the UI tabs and within-tab order feel familiar.
const std::vector<OptionDef> &schemaAll();

// Distinct sections in display order.
const std::vector<std::string> &schemaSections();

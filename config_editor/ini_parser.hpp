// Preservation-aware INI parser.
//
// Unlike Config::load() in the main DLL, this parser keeps every original
// line - including comments, blanks, fancy ASCII boxes, inline `;`-suffix
// comments - so that save() can rewrite ONLY the key=value lines and leave
// all of the bilingual documentation intact.
//
// Layout:
//   Ini ini;
//   ini.load("angle_config.ini");
//   ini.set("BoostLatency", "frame_pacing", "true");
//   ini.save("angle_config.ini");   // 654 of 655 lines unchanged

#pragma once
#include <string>
#include <vector>

struct IniLine {
  enum Kind { Comment, Section, KeyValue, Blank };
  Kind kind = Blank;
  std::string raw;             // verbatim (used for Comment / Blank / Section)
  std::string section;         // for KeyValue: which [Section] this lives in
  std::string key;             // for KeyValue
  std::string value;           // for KeyValue (mutable)
  std::string trailingComment; // optional `; …` after the value on same line
  std::string indent; // leading whitespace before key (rare in our INI)
};

class Ini {
public:
  bool load(const std::string &path);
  bool save(const std::string &path) const;

  // Lookup. Returns empty string if missing.
  std::string get(const std::string &section, const std::string &key) const;

  // Mutate. Adds the line if it doesn't exist (rare - usually we only edit
  // lines that already exist in the bilingual template).
  void set(const std::string &section, const std::string &key,
           const std::string &value);

  bool has(const std::string &section, const std::string &key) const;

  const std::vector<IniLine> &lines() const { return m_lines; }

private:
  std::vector<IniLine> m_lines;
};

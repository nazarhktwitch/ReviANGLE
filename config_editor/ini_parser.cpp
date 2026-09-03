#include "ini_parser.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>


namespace {

std::string trim(std::string s) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
  s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
  return s;
}

std::string leadingWhitespace(const std::string &s) {
  size_t i = 0;
  while (i < s.size() && std::isspace((unsigned char)s[i]))
    ++i;
  return s.substr(0, i);
}

bool ieq(const std::string &a, const std::string &b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i]))
      return false;
  }
  return true;
}

} // namespace

bool Ini::load(const std::string &path) {
  m_lines.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open())
    return false;

  std::string section;
  std::string raw;
  while (std::getline(f, raw)) {
    // Strip optional trailing \r from CRLF input.
    if (!raw.empty() && raw.back() == '\r')
      raw.pop_back();

    IniLine ln;
    ln.raw = raw;

    std::string trimmed = trim(raw);
    if (trimmed.empty()) {
      ln.kind = IniLine::Blank;
      m_lines.push_back(std::move(ln));
      continue;
    }
    if (trimmed[0] == ';' || trimmed[0] == '#') {
      ln.kind = IniLine::Comment;
      m_lines.push_back(std::move(ln));
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      ln.kind = IniLine::Section;
      section = trimmed.substr(1, trimmed.size() - 2);
      ln.section = section;
      m_lines.push_back(std::move(ln));
      continue;
    }

    // key=value (with optional inline `; trailing comment`).
    auto eq = trimmed.find('=');
    if (eq == std::string::npos) {
      // Malformed - keep verbatim as a comment-ish blob.
      ln.kind = IniLine::Comment;
      m_lines.push_back(std::move(ln));
      continue;
    }

    std::string key = trim(trimmed.substr(0, eq));
    std::string rest = trimmed.substr(eq + 1);

    // Detect inline `; comment` suffix while respecting quoted strings
    // (we don't currently quote anything, but be defensive).
    std::string val = rest;
    std::string trailing;
    bool inQuotes = false;
    for (size_t i = 0; i < rest.size(); ++i) {
      char c = rest[i];
      if (c == '"')
        inQuotes = !inQuotes;
      else if ((c == ';' || c == '#') && !inQuotes) {
        val = trim(rest.substr(0, i));
        trailing = rest.substr(i); // includes the `;` itself
        break;
      }
    }
    val = trim(val);

    ln.kind = IniLine::KeyValue;
    ln.section = section;
    ln.key = key;
    ln.value = val;
    ln.trailingComment = trailing;
    ln.indent = leadingWhitespace(raw);
    m_lines.push_back(std::move(ln));
  }
  return true;
}

bool Ini::save(const std::string &path) const {
  // Two-pass write to a temp file, then rename for atomicity (best-effort
  // - falls back to direct write if rename fails).
  std::string tmp = path + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f.is_open())
      return false;

    for (const auto &ln : m_lines) {
      switch (ln.kind) {
      case IniLine::Blank:
      case IniLine::Comment:
      case IniLine::Section:
        f << ln.raw << "\r\n";
        break;
      case IniLine::KeyValue: {
        f << ln.indent << ln.key << "=" << ln.value;
        if (!ln.trailingComment.empty()) {
          // Preserve the original spacing style: a single space
          // before `;` if user had any, otherwise direct.
          if (ln.trailingComment.front() != ' ')
            f << " ";
          f << ln.trailingComment;
        }
        f << "\r\n";
        break;
      }
      }
    }
  }

  // Atomic-ish replace: remove old, rename tmp.
  std::remove(path.c_str());
  if (std::rename(tmp.c_str(), path.c_str()) != 0) {
    // Fallback: copy contents directly.
    std::ifstream src(tmp, std::ios::binary);
    std::ofstream dst(path, std::ios::binary | std::ios::trunc);
    if (!src.is_open() || !dst.is_open())
      return false;
    dst << src.rdbuf();
    std::remove(tmp.c_str());
  }
  return true;
}

std::string Ini::get(const std::string &section, const std::string &key) const {
  for (const auto &ln : m_lines) {
    if (ln.kind == IniLine::KeyValue && ieq(ln.section, section) &&
        ieq(ln.key, key))
      return ln.value;
  }
  return {};
}

bool Ini::has(const std::string &section, const std::string &key) const {
  for (const auto &ln : m_lines) {
    if (ln.kind == IniLine::KeyValue && ieq(ln.section, section) &&
        ieq(ln.key, key))
      return true;
  }
  return false;
}

void Ini::set(const std::string &section, const std::string &key,
              const std::string &value) {
  for (auto &ln : m_lines) {
    if (ln.kind == IniLine::KeyValue && ieq(ln.section, section) &&
        ieq(ln.key, key)) {
      ln.value = value;
      return;
    }
  }
  // Key missing - append at end of its section, or at file end.
  // Find the last line in that section.
  int insertAt = -1;
  bool inSection = false;
  for (size_t i = 0; i < m_lines.size(); ++i) {
    const auto &ln = m_lines[i];
    if (ln.kind == IniLine::Section) {
      if (ieq(ln.section, section))
        inSection = true;
      else if (inSection) {
        insertAt = (int)i;
        break;
      }
    }
  }
  IniLine fresh;
  fresh.kind = IniLine::KeyValue;
  fresh.section = section;
  fresh.key = key;
  fresh.value = value;
  if (insertAt < 0) {
    // Section never seen - emit a fresh [Section] header + key.
    if (!m_lines.empty() && m_lines.back().kind != IniLine::Blank) {
      IniLine blank;
      blank.kind = IniLine::Blank;
      m_lines.push_back(blank);
    }
    IniLine hdr;
    hdr.kind = IniLine::Section;
    hdr.section = section;
    hdr.raw = "[" + section + "]";
    m_lines.push_back(hdr);
    m_lines.push_back(fresh);
  } else {
    m_lines.insert(m_lines.begin() + insertAt, fresh);
  }
}

#include "core/valve_key_values.h"

#include <cctype>
#include <cstdlib>
#include <utility>

namespace gameslauncher::vdf {
namespace {

constexpr int kMaxDepth = 32;

class Parser {
public:
  Parser(std::string_view text, std::string* error) : m_text(text), m_error(error) {}

  std::optional<Node> run() {
    Node root;
    root.is_object = true;
    if (!parse_entries(&root, true)) {
      return std::nullopt;
    }
    skip_space_and_comments();
    if (m_pos != m_text.size()) {
      fail("trailing data");
      return std::nullopt;
    }
    return root;
  }

private:
  void fail(const std::string& message) {
    if (m_error != nullptr && m_error->empty()) {
      *m_error = message + " at byte " + std::to_string(m_pos);
    }
  }

  void skip_space_and_comments() {
    while (m_pos < m_text.size()) {
      const unsigned char ch = static_cast<unsigned char>(m_text[m_pos]);
      if (std::isspace(ch) != 0) {
        ++m_pos;
        continue;
      }
      if (ch == '/' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '/') {
        m_pos += 2;
        while (m_pos < m_text.size() && m_text[m_pos] != '\n') {
          ++m_pos;
        }
        continue;
      }
      break;
    }
  }

  bool parse_entries(Node* object, bool top_level) {
    if (++m_depth > kMaxDepth) {
      fail("nesting too deep");
      return false;
    }
    while (true) {
      skip_space_and_comments();
      if (m_pos >= m_text.size()) {
        if (!top_level) {
          fail("unexpected end of object");
          --m_depth;
          return false;
        }
        --m_depth;
        return true;
      }
      if (m_text[m_pos] == '}') {
        if (top_level) {
          fail("unexpected closing brace");
          --m_depth;
          return false;
        }
        ++m_pos;
        --m_depth;
        return true;
      }

      std::string key;
      if (!parse_token(&key)) {
        --m_depth;
        return false;
      }
      skip_space_and_comments();
      if (m_pos >= m_text.size()) {
        fail("missing value");
        --m_depth;
        return false;
      }
      if (m_text[m_pos] == '{') {
        ++m_pos;
        Node child;
        child.is_object = true;
        if (!parse_entries(&child, false)) {
          --m_depth;
          return false;
        }
        object->children[std::move(key)] = std::move(child);
      } else {
        std::string value;
        if (!parse_token(&value)) {
          --m_depth;
          return false;
        }
        Node child;
        child.value = std::move(value);
        object->children[std::move(key)] = std::move(child);
      }
    }
  }

  bool parse_token(std::string* out) {
    skip_space_and_comments();
    if (m_pos >= m_text.size()) {
      fail("expected token");
      return false;
    }
    if (m_text[m_pos] == '"') {
      ++m_pos;
      while (m_pos < m_text.size()) {
        const char ch = m_text[m_pos++];
        if (ch == '"') {
          return true;
        }
        if (ch == '\\' && m_pos < m_text.size()) {
          const char escaped = m_text[m_pos++];
          switch (escaped) {
          case 'n':
            out->push_back('\n');
            break;
          case 't':
            out->push_back('\t');
            break;
          case '\\':
          case '"':
            out->push_back(escaped);
            break;
          default:
            out->push_back(escaped);
            break;
          }
          continue;
        }
        out->push_back(ch);
      }
      fail("unterminated string");
      return false;
    }

    const std::size_t start = m_pos;
    while (m_pos < m_text.size()) {
      const unsigned char ch = static_cast<unsigned char>(m_text[m_pos]);
      if (std::isspace(ch) != 0 || ch == '{' || ch == '}' || ch == '"') {
        break;
      }
      ++m_pos;
    }
    if (m_pos == start) {
      fail("invalid token");
      return false;
    }
    out->assign(m_text.substr(start, m_pos - start));
    return true;
  }

  std::string_view m_text;
  std::string* m_error = nullptr;
  std::size_t m_pos = 0;
  int m_depth = 0;
};

} // namespace

const Node* Node::child(std::string_view key) const {
  const auto it = children.find(std::string(key));
  return it == children.end() ? nullptr : &it->second;
}

std::string Node::string_value(std::string_view key) const {
  const Node* node = child(key);
  return node == nullptr ? std::string() : node->value;
}

long long Node::integer_value(std::string_view key, long long fallback) const {
  const std::string value = string_value(key);
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long long parsed = std::strtoll(value.c_str(), &end, 10);
  if (end == value.c_str()) {
    return fallback;
  }
  return parsed;
}

std::optional<Node> parse(std::string_view text, std::string* error) {
  if (error != nullptr) {
    error->clear();
  }
  Parser parser(text, error);
  return parser.run();
}

} // namespace gameslauncher::vdf

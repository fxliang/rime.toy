#pragma once

#include <string>
#include <vector>

enum TextAttributeType { NONE = 0, HIGHLIGHTED, LAST_TYPE };

struct TextRange {
  TextRange() : start(0), end(0), cursor(-1) {}
  TextRange(int _start, int _end, int _cursor)
      : start(_start), end(_end), cursor(_cursor) {}
  bool operator==(const TextRange &tr) {
    return (start == tr.start && end == tr.end && cursor == tr.cursor);
  }
  bool operator!=(const TextRange &tr) {
    return (start != tr.start || end != tr.end || cursor != tr.cursor);
  }
  int start;
  int end;
  int cursor;
};

struct TextAttribute {
  TextAttribute() : type(NONE) {}
  TextAttribute(int _start, int _end, TextAttributeType _type)
      : range(_start, _end, -1), type(_type) {}
  bool operator==(const TextAttribute &ta) {
    return (range == ta.range && type == ta.type);
  }
  bool operator!=(const TextAttribute &ta) {
    return (range != ta.range || type != ta.type);
  }
  TextRange range;
  TextAttributeType type;
};

struct Text {
  Text() : str(L"") {}
  Text(std::wstring const &_str) : str(_str) {}
  void clear() {
    str.clear();
    attributes.clear();
  }
  bool empty() const { return str.empty(); }
  bool operator==(const Text &txt) {
    if (str != txt.str || (attributes.size() != txt.attributes.size()))
      return false;
    for (size_t i = 0; i < attributes.size(); i++) {
      if ((attributes[i] != txt.attributes[i]))
        return false;
    }
    return true;
  }
  bool operator!=(const Text &txt) {
    if (str != txt.str || (attributes.size() != txt.attributes.size()))
      return true;
    for (size_t i = 0; i < attributes.size(); i++) {
      if ((attributes[i] != txt.attributes[i]))
        return true;
    }
    return false;
  }
  std::wstring str;
  std::vector<TextAttribute> attributes;
};

struct CandidateInfo {
  CandidateInfo() {
    currentPage = 0;
    totalPages = 0;
    highlighted = 0;
    is_last_page = false;
  }
  void clear() {
    currentPage = 0;
    totalPages = 0;
    highlighted = 0;
    is_last_page = false;
    candies.clear();
    labels.clear();
  }
  bool empty() const { return candies.empty(); }
  bool operator==(const CandidateInfo &ci) {
    if (currentPage != ci.currentPage || totalPages != ci.totalPages ||
        highlighted != ci.highlighted || is_last_page != ci.is_last_page ||
        notequal(candies, ci.candies) || notequal(comments, ci.comments) ||
        notequal(labels, ci.labels))
      return false;
    return true;
  }
  bool operator!=(const CandidateInfo &ci) {
    if (currentPage != ci.currentPage || totalPages != ci.totalPages ||
        highlighted != ci.highlighted || is_last_page != ci.is_last_page ||
        notequal(candies, ci.candies) || notequal(comments, ci.comments) ||
        notequal(labels, ci.labels))
      return true;
    return false;
  }
  bool notequal(std::vector<Text> txtSrc, std::vector<Text> txtDst) {
    if (txtSrc.size() != txtDst.size())
      return true;
    for (size_t i = 0; i < txtSrc.size(); i++) {
      if (txtSrc[i] != txtDst[i])
        return true;
    }
    return false;
  }
  int currentPage;
  bool is_last_page;
  int totalPages;
  int highlighted;
  std::vector<Text> candies;
  std::vector<Text> comments;
  std::vector<Text> labels;
};

struct Context {
  Context() {}
  void clear() {
    preedit.clear();
    aux.clear();
    cinfo.clear();
  }
  bool empty() const { return preedit.empty() && aux.empty() && cinfo.empty(); }
  bool operator==(const Context &ctx) {
    if (preedit == ctx.preedit && aux == ctx.aux && cinfo == ctx.cinfo)
      return true;
    return false;
  }
  bool operator!=(const Context &ctx) { return !(operator==(ctx)); }

  bool operator!() {
    if (preedit.str.empty() && aux.str.empty() && cinfo.candies.empty() &&
        cinfo.labels.empty() && cinfo.comments.empty())
      return true;
    else
      return false;
  }
  Text preedit;
  Text aux;
  CandidateInfo cinfo;
};

struct Status {
  Status()
      : ascii_mode(false), composing(false), disabled(false),
        full_shape(false) {}
  void reset() {
    schema_name.clear();
    schema_id.clear();
    ascii_mode = false;
    composing = false;
    disabled = false;
    full_shape = false;
  }
  bool operator==(const Status status) {
    return (status.schema_name == schema_name &&
            status.schema_id == schema_id && status.ascii_mode == ascii_mode &&
            status.composing == composing && status.disabled == disabled &&
            status.full_shape == full_shape);
  }
  // 輸入方案
  std::wstring schema_name;
  // 輸入方案 id
  std::wstring schema_id;
  // 轉換開關
  bool ascii_mode;
  // 寫作狀態
  bool composing;
  // 維護模式（暫停輸入功能）
  bool disabled;
  // 全角状态
  bool full_shape;
};

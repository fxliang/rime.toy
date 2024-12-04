#include "StandardLayout.h"

using namespace weasel;

wstring StandardLayout::GetLabelText(const vector<Text> &labels, int id,
                                     const wchar_t *format) const {
  wchar_t buffer[128];
  swprintf_s<128>(buffer, format, labels.at(id).str.c_str());
  return std::wstring(buffer);
}

CSize StandardLayout::GetPreeditSize(const Text &text,
                                     ComPtr<IDWriteTextFormat1> &pTextFormat) {
  const wstring &preedit = text.str;
  const vector<TextAttribute> &attrs = text.attributes;
  CSize size(0, 0);
  if (!preedit.empty()) {
    weasel::TextRange range;
    for (size_t j = 0; j < attrs.size(); ++j)
      if (attrs[j].type == weasel::HIGHLIGHTED)
        _range = attrs[j].range;
    if (_range.start < _range.end) {
      wstring before_str = preedit.substr(0, _range.start);
      wstring hilited_str = preedit.substr(_range.start, _range.end);
      wstring after_str = preedit.substr(_range.end);
      _pD2D->GetTextSize(before_str, before_str.length(), pTextFormat,
                         &_beforesz);
      _pD2D->GetTextSize(hilited_str, hilited_str.length(), pTextFormat,
                         &_hilitedsz);
      _pD2D->GetTextSize(after_str, after_str.length(), pTextFormat, &_aftersz);
      auto width_max = 0, height_max = 0;
      if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
        width_max = MAX(width_max, _beforesz.cx);
        width_max = MAX(width_max, _hilitedsz.cx);
        width_max = MAX(width_max, _aftersz.cx);
        height_max += _beforesz.cy + (_beforesz.cy > 0) * _style.hilite_spacing;
        height_max +=
            _hilitedsz.cy + (_hilitedsz.cy > 0) * _style.hilite_spacing;
        height_max += _aftersz.cy;
      } else {
        height_max = MAX(height_max, _beforesz.cy);
        height_max = MAX(height_max, _hilitedsz.cy);
        height_max = MAX(height_max, _aftersz.cy);
        width_max += _beforesz.cx + (_beforesz.cx > 0) * _style.hilite_spacing;
        width_max +=
            _hilitedsz.cx + (_hilitedsz.cx > 0) * _style.hilite_spacing;
        width_max += _aftersz.cx;
      }
      size.cx = width_max;
      size.cy = height_max;
    } else
      _pD2D->GetTextSize(preedit, preedit.length(), pTextFormat, &size);
  }
  return size;
}

void StandardLayout::UpdateStatusIconLayout(int *width, int *height) {
  // rule 1. status icon is middle-aligned with preedit text or auxiliary text,
  // whichever comes first rule 2. there is a spacing between preedit/aux text
  // and the status icon rule 3. status icon is right aligned in WeaselPanel,
  // when [margin_x + width(preedit/aux) + spacing + width(icon) + margin_x] <
  // style.min_width
  if (ShouldDisplayStatusIcon()) {
    if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
      int top = 0, middle = 0;
      if (!_preeditRect.IsRectNull()) {
        top = _preeditRect.bottom + _style.spacing;
        middle = (_preeditRect.left + _preeditRect.right) / 2;
      } else if (!_auxiliaryRect.IsRectNull()) {
        top = _auxiliaryRect.bottom + _style.spacing;
        middle = (_auxiliaryRect.left + _auxiliaryRect.right) / 2;
      }
      if (top && middle) {
        int bottom_alignment = *height - real_margin_y - STATUS_ICON_SIZE;
        if (top > bottom_alignment) {
          *height = top + STATUS_ICON_SIZE + real_margin_y;
        } else {
          top = bottom_alignment;
        }
        _statusIconRect.SetRect(middle - STATUS_ICON_SIZE / 2 + 1, top,
                                middle + STATUS_ICON_SIZE / 2 + 1,
                                top + STATUS_ICON_SIZE + 1);
      } else {
        _statusIconRect.SetRect(0, 0, STATUS_ICON_SIZE, STATUS_ICON_SIZE);
        _statusIconRect.OffsetRect(offsetX, offsetY);
        *width = STATUS_ICON_SIZE;
        *height =
            (_style.vertical_text_with_wrap ? offsetY : 0) + STATUS_ICON_SIZE;
      }
    } else {
      int left = 0, middle = 0;
      if (!_preeditRect.IsRectNull()) {
        left = _preeditRect.right + _style.spacing;
        middle = (_preeditRect.top + _preeditRect.bottom) / 2;
      } else if (!_auxiliaryRect.IsRectNull()) {
        left = _auxiliaryRect.right + _style.spacing;
        middle = (_auxiliaryRect.top + _auxiliaryRect.bottom) / 2;
      }
      if (left && middle) {
        int right_alignment = *width - real_margin_x - STATUS_ICON_SIZE;
        if (left > right_alignment) {
          *width = left + STATUS_ICON_SIZE + real_margin_x;
        } else {
          left = right_alignment;
        }
        _statusIconRect.SetRect(left, middle - STATUS_ICON_SIZE / 2 + 1,
                                left + STATUS_ICON_SIZE,
                                middle + STATUS_ICON_SIZE / 2 + 1);
      } else {
        _statusIconRect.SetRect(0, 0, STATUS_ICON_SIZE, STATUS_ICON_SIZE);
        _statusIconRect.OffsetRect(offsetX, offsetY);
        *width = *height = STATUS_ICON_SIZE;
      }
    }
  }
  if (IS_FULLSCREENLAYOUT(_style))
    _statusIconRect.OffsetRect(-_style.border, -_style.border);
}

bool StandardLayout::IsInlinePreedit() const {
  return _style.inline_preedit &&
         (_style.client_caps & weasel::INLINE_PREEDIT_CAPABLE) != 0 &&
         _style.layout_type != UIStyle::LAYOUT_VERTICAL_FULLSCREEN &&
         _style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN;
}

bool StandardLayout::ShouldDisplayStatusIcon() const {
  // rule 1. emphasis ascii mode
  // rule 2. show status icon when switching mode
  // rule 3. always show status icon with tips
  // rule 4. rule 3 excluding tips FullScreenLayout with strings
  return ((_status.ascii_mode && !_style.inline_preedit) ||
          !_status.composing || !_context.aux.empty()) &&
         !((_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN ||
            _style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN) &&
           !_context.aux.empty());
}

bool StandardLayout::_IsHighlightOverCandidateWindow(const CRect &rc) {
  return (rc.left <= _bgRect.left || rc.right >= _bgRect.right ||
          rc.top <= _bgRect.top || rc.bottom >= _bgRect.bottom);
}

void StandardLayout::_PrepareRoundInfo() {

  const int tmp[5] = {UIStyle::LAYOUT_VERTICAL, UIStyle::LAYOUT_HORIZONTAL,
                      UIStyle::LAYOUT_VERTICAL_TEXT, UIStyle::LAYOUT_VERTICAL,
                      UIStyle::LAYOUT_HORIZONTAL};
  int layout_type = tmp[_style.layout_type];
  bool textHemispherical = false, cand0Hemispherical = false;
  if (!_style.inline_preedit) {
    CRect textRect(_preeditRect);
    textRect.InflateRect(_style.hilite_padding_x, _style.hilite_padding_y);
    textHemispherical = _IsHighlightOverCandidateWindow(textRect);
    const bool hilite_rd_info[3][2][4] = {
        // vertical
        {{textHemispherical, textHemispherical && (!candidates_count), false,
          false},
         {true, true, true, true}},
        // horizontal
        {{textHemispherical, textHemispherical && (!candidates_count), false,
          false},
         {true, true, true, true}},
        // vertical text
        {{textHemispherical && (!candidates_count), false, textHemispherical,
          false},
         {true, true, true, true}}};

    _textRoundInfo.IsTopLeftNeedToRound =
        hilite_rd_info[layout_type][!textHemispherical][0];
    _textRoundInfo.IsBottomLeftNeedToRound =
        hilite_rd_info[layout_type][!textHemispherical][1];
    _textRoundInfo.IsTopRightNeedToRound =
        hilite_rd_info[layout_type][!textHemispherical][2];
    _textRoundInfo.IsBottomRightNeedToRound =
        hilite_rd_info[layout_type][!textHemispherical][3];
    _textRoundInfo.Hemispherical = textHemispherical;
    if (_style.vertical_text_left_to_right &&
        _style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
      std::swap(_textRoundInfo.IsTopLeftNeedToRound,
                _textRoundInfo.IsTopRightNeedToRound);
    }
  }

  if (candidates_count) {
    CRect cand0Rect(_candidateRects[0]);
    cand0Rect.InflateRect(_style.hilite_padding_x, _style.hilite_padding_y);
    cand0Hemispherical = _IsHighlightOverCandidateWindow(cand0Rect);
    if (textHemispherical || cand0Hemispherical) {
      // if (current_hemispherical_dome_status) _textRoundInfo.Hemispherical =
      // true;
      //  level 0: m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL
      //  level 1: m_style.inline_preedit
      //  level 2: BackType
      //  level 3: IsTopLeftNeedToRound, IsBottomLeftNeedToRound,
      //  IsTopRightNeedToRound, IsBottomRightNeedToRound
      const static bool is_to_round_corner[3][2][5][4] = {
          // LAYOUT_VERTICAL
          {// not inline_preedit
           {
               {false, false, false, false}, // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {false, true, false, true},   // LAST_CAND
               {false, true, false, true},   // ONLY_CAND
           },
           // inline_preedit
           {
               {true, false, true, false},   // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {false, true, false, true},   // LAST_CAND
               {true, true, true, true},     // ONLY_CAND
           }},
          // LAYOUT_HORIZONTAL
          {// not inline_preedit
           {
               {false, true, false, false},  // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {false, false, false, true},  // LAST_CAND
               {false, true, false, true},   // ONLY_CAND
           },
           // inline_preedit
           {
               {true, true, false, false},   // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {false, false, true, true},   // LAST_CAND
               {true, true, true, true},     // ONLY_CAND
           }},
          // LAYOUT_VERTICAL_TEXT
          {// not inline_preedit
           {
               {false, false, false, false}, // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {true, true, false, false},   // LAST_CAND
               {false, false, true, true},   // ONLY_CAND
           },
           // inline_preedit
           {
               {false, false, true, true},   // FIRST_CAND
               {false, false, false, false}, // MID_CAND
               {true, true, false, false},   // LAST_CAND
               {true, true, true, true},     // ONLY_CAND
           }},
      };
      for (auto i = 0; i < candidates_count; i++) {
        CRect hilite_rect(_candidateRects[i]);
        hilite_rect.InflateRect(_style.hilite_padding_x,
                                _style.hilite_padding_y);
        bool current_hemispherical_dome_status =
            _IsHighlightOverCandidateWindow(hilite_rect);
        int type = 0;              // default FIRST_CAND
        if (candidates_count == 1) // ONLY_CAND
          type = 3;
        else if (i != 0 && i != candidates_count - 1) // MID_CAND
          type = 1;
        else if (i == candidates_count - 1) // LAST_CAND
          type = 2;
        _roundInfo[i].IsTopLeftNeedToRound =
            is_to_round_corner[layout_type][_style.inline_preedit][type][0];
        _roundInfo[i].IsBottomLeftNeedToRound =
            is_to_round_corner[layout_type][_style.inline_preedit][type][1];
        _roundInfo[i].IsTopRightNeedToRound =
            is_to_round_corner[layout_type][_style.inline_preedit][type][2];
        _roundInfo[i].IsBottomRightNeedToRound =
            is_to_round_corner[layout_type][_style.inline_preedit][type][3];
        _roundInfo[i].Hemispherical = current_hemispherical_dome_status;
      }
      // fix round info for vetical text layout when vertical_text_left_to_right
      // is set
      if (_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT &&
          _style.vertical_text_left_to_right) {
        if (_style.inline_preedit) {
          if (candidates_count > 1) {
            _roundInfo[candidates_count - 1].IsTopLeftNeedToRound =
                !_roundInfo[candidates_count - 1].IsTopLeftNeedToRound;
            _roundInfo[candidates_count - 1].IsTopRightNeedToRound =
                !_roundInfo[candidates_count - 1].IsTopRightNeedToRound;
            _roundInfo[candidates_count - 1].IsBottomLeftNeedToRound =
                !_roundInfo[candidates_count - 1].IsBottomLeftNeedToRound;
            _roundInfo[candidates_count - 1].IsBottomRightNeedToRound =
                !_roundInfo[candidates_count - 1].IsBottomRightNeedToRound;
            _roundInfo[0].IsTopLeftNeedToRound =
                !_roundInfo[0].IsTopLeftNeedToRound;
            _roundInfo[0].IsTopRightNeedToRound =
                !_roundInfo[0].IsTopRightNeedToRound;
            _roundInfo[0].IsBottomLeftNeedToRound =
                !_roundInfo[0].IsBottomLeftNeedToRound;
            _roundInfo[0].IsBottomRightNeedToRound =
                !_roundInfo[0].IsBottomRightNeedToRound;
          }
        } else {
          if (candidates_count > 1) {
            _roundInfo[candidates_count - 1].IsTopLeftNeedToRound =
                !_roundInfo[candidates_count - 1].IsTopLeftNeedToRound;
            _roundInfo[candidates_count - 1].IsTopRightNeedToRound =
                !_roundInfo[candidates_count - 1].IsTopRightNeedToRound;
            _roundInfo[candidates_count - 1].IsBottomLeftNeedToRound =
                !_roundInfo[candidates_count - 1].IsBottomLeftNeedToRound;
            _roundInfo[candidates_count - 1].IsBottomRightNeedToRound =
                !_roundInfo[candidates_count - 1].IsBottomRightNeedToRound;
          }
        }
      }
    }
  }
}

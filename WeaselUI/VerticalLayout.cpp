#include "VerticalLayout.h"

using namespace weasel;

void weasel::VerticalLayout::DoLayout() {
  const int space = _style.hilite_spacing;
  int width = 0, height = real_margin_y;

  if ((_style.hilited_mark_color & 0xff000000)) {
    CSize sg;
    if (candidates_count) {
      if (_style.mark_text.empty())
        _pD2D->GetTextSize(L"|", 1, _pD2D->pTextFormat, &sg);
      else
        _pD2D->GetTextSize(_style.mark_text, _style.mark_text.length(),
                           _pD2D->pTextFormat, &sg);
    }

    mark_width = sg.cx;
    mark_height = sg.cy;
    if (_style.mark_text.empty()) {
      mark_width = mark_height / 7;
      if (_style.linespacing && _style.baseline)
        mark_width =
            (int)((float)mark_width / ((float)_style.linespacing / 100.0f));
      mark_width = MAX(mark_width, 6);
    }
    mark_gap = (_style.mark_text.empty()) ? mark_width
                                          : mark_width + _style.hilite_spacing;
  }
  int base_offset = ((_style.hilited_mark_color & 0xff000000)) ? mark_gap : 0;

  // calc page indicator
  CSize pgszl, pgszr;
  if (!IsInlinePreedit()) {
    _pD2D->GetTextSize(pre, pre.length(), _pD2D->pPreeditFormat, &pgszl);
    _pD2D->GetTextSize(next, next.length(), _pD2D->pPreeditFormat, &pgszr);
  }
  bool page_en = (_style.prevpage_color & 0xff000000) &&
                 (_style.nextpage_color & 0xff000000);
  int pgw = page_en ? pgszl.cx + pgszr.cx + _style.hilite_spacing +
                          _style.hilite_padding_x * 2
                    : 0;
  int pgh = page_en ? MAX(pgszl.cy, pgszr.cy) : 0;

  /*  preedit and auxiliary rectangle calc start */
  CSize size;
  /* Preedit */
  if (!IsInlinePreedit() && !_context.preedit.str.empty()) {
    size = GetPreeditSize(_context.preedit, _pD2D->pPreeditFormat);
    int szx = pgw, szy = MAX(size.cy, pgh);
    // icon size higher then preedit text
    int yoffset = (STATUS_ICON_SIZE >= szy && ShouldDisplayStatusIcon())
                      ? (STATUS_ICON_SIZE - szy) / 2
                      : 0;
    _preeditRect.SetRect(real_margin_x, height + yoffset,
                         real_margin_x + size.cx, height + yoffset + size.cy);
    height += szy + 2 * yoffset + _style.spacing;
    width = MAX(width, real_margin_x * 2 + size.cx + szx);
    if (ShouldDisplayStatusIcon())
      width += STATUS_ICON_SIZE;
    _preeditRect.OffsetRect(offsetX, offsetY);
  }

  /* Auxiliary */
  if (!_context.aux.str.empty()) {
    size = GetPreeditSize(_context.aux, _pD2D->pPreeditFormat);
    // icon size higher then auxiliary text
    int yoffset = (STATUS_ICON_SIZE >= size.cy && ShouldDisplayStatusIcon())
                      ? (STATUS_ICON_SIZE - size.cy) / 2
                      : 0;
    _auxiliaryRect.SetRect(real_margin_x, height + yoffset,
                           real_margin_x + size.cx, height + yoffset + size.cy);
    height += size.cy + 2 * yoffset + _style.spacing;
    width = MAX(width, real_margin_x * 2 + size.cx);
    _auxiliaryRect.OffsetRect(offsetX, offsetY);
  }
  /*  preedit and auxiliary rectangle calc end */

  /* Candidates */
  int comment_shift_width = 0; /* distance to the left of the candidate text */
  int max_candidate_width = 0; /* label + text */
  int max_comment_width = 0;   /* comment, or none */
  for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
    if (i > 0)
      height += _style.candidate_spacing;

    int w = real_margin_x + base_offset, max_height_curren_candidate = 0;
    int candidate_width = base_offset, comment_width = 0;
    /* Label */
    auto &label = labels.at(i).str;
    _pD2D->GetTextSize(label, label.length(), _pD2D->pLabelFormat, &size);
    _candidateLabelRects[i].SetRect(w, height, w + size.cx * labelFontValid,
                                    height + size.cy);
    _candidateLabelRects[i].OffsetRect(offsetX, offsetY);
    w += (size.cx + space) * labelFontValid;
    max_height_curren_candidate = MAX(max_height_curren_candidate, size.cy);
    candidate_width += (size.cx + space) * labelFontValid;

    /* Text */
    const std::wstring &text = candidates.at(i).str;
    _pD2D->GetTextSize(text, text.length(), _pD2D->pTextFormat, &size);
    _candidateTextRects[i].SetRect(w, height, w + size.cx * textFontValid,
                                   height + size.cy);
    _candidateTextRects[i].OffsetRect(offsetX, offsetY);
    w += size.cx * textFontValid;
    max_height_curren_candidate = MAX(max_height_curren_candidate, size.cy);
    candidate_width += size.cx * textFontValid;
    max_candidate_width = MAX(max_candidate_width, candidate_width);

    /* Comment */
    bool cmtFontNotTrans =
        (i == id && (_style.hilited_comment_text_color & 0xff000000)) ||
        (i != id && (_style.comment_text_color & 0xff000000));
    if (!comments.at(i).str.empty() && cmtFontValid && cmtFontNotTrans) {
      w += space;
      comment_shift_width = MAX(comment_shift_width, w);

      const std::wstring &comment = comments.at(i).str;
      _pD2D->GetTextSize(comment, comment.length(), _pD2D->pCommentFormat,
                         &size);
      _candidateCommentRects[i].SetRect(0, height, size.cx * cmtFontValid,
                                        height + size.cy);
      _candidateCommentRects[i].OffsetRect(offsetX, offsetY);
      w += size.cx * cmtFontValid;
      max_height_curren_candidate = MAX(max_height_curren_candidate, size.cy);
      comment_width += size.cx * cmtFontValid;
      max_comment_width = MAX(max_comment_width, comment_width);
    }
    int ol = 0, ot = 0, oc = 0;
    if (_style.align_type == UIStyle::ALIGN_CENTER) {
      ol = (max_height_curren_candidate - _candidateLabelRects[i].Height()) / 2;
      ot = (max_height_curren_candidate - _candidateTextRects[i].Height()) / 2;
      oc = (max_height_curren_candidate - _candidateCommentRects[i].Height()) /
           2;
    } else if (_style.align_type == UIStyle::ALIGN_BOTTOM) {
      ol = (max_height_curren_candidate - _candidateLabelRects[i].Height());
      ot = (max_height_curren_candidate - _candidateTextRects[i].Height());
      oc = (max_height_curren_candidate - _candidateCommentRects[i].Height());
    }
    _candidateLabelRects[i].OffsetRect(0, ol);
    _candidateTextRects[i].OffsetRect(0, ot);
    _candidateCommentRects[i].OffsetRect(0, oc);

    int hlTop = _candidateTextRects[i].top;
    int hlBot = _candidateTextRects[i].bottom;
    if (_candidateLabelRects[i].Height() > 0) {
      hlTop = MIN(_candidateLabelRects[i].top, hlTop);
      hlBot =
          MAX(_candidateLabelRects[i].bottom, _candidateTextRects[i].bottom);
    }
    if (_candidateCommentRects[i].Height() > 0) {
      hlTop = MIN(hlTop, _candidateCommentRects[i].top);
      hlBot = MAX(hlBot, _candidateCommentRects[i].bottom);
    }
    _candidateRects[i].SetRect(real_margin_x + offsetX, hlTop,
                               width - real_margin_x + offsetX, hlBot);

    width = MAX(width, w);
    height += max_height_curren_candidate;
  }
  /* comments are left-aligned to the right of the longest candidate who has a
   * comment */
  int max_content_width =
      MAX(max_candidate_width, comment_shift_width + max_comment_width);
  width = MAX(width, max_content_width + 2 * real_margin_x);

  /* Align comments */
  for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
    int hlTop = _candidateTextRects[i].top;
    int hlBot = _candidateTextRects[i].bottom;

    _candidateCommentRects[i].OffsetRect(real_margin_x + comment_shift_width,
                                         0);
    if (_candidateLabelRects[i].Height() > 0) {
      hlTop = MIN(_candidateLabelRects[i].top, hlTop);
      hlBot =
          MAX(_candidateLabelRects[i].bottom, _candidateTextRects[i].bottom);
    }
    if (_candidateCommentRects[i].Height() > 0) {
      hlTop = MIN(hlTop, _candidateCommentRects[i].top);
      hlBot = MAX(hlBot, _candidateCommentRects[i].bottom);
    }

    _candidateRects[i].SetRect(real_margin_x + offsetX, hlTop,
                               width - real_margin_x + offsetX, hlBot);
  }

  /* Trim the last spacing if no candidates */
  if (candidates_count == 0)
    height -= _style.spacing;

  height += real_margin_y;

  if (candidates_count) {
    width = MAX(width, _style.min_width);
    height = MAX(height, _style.min_height);
  }
  UpdateStatusIconLayout(&width, &height);
  // candidate rectangle always align to right side, margin_x to the right edge
  for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
    int right =
        MAX(_candidateRects[i].right,
            _candidateRects[i].left - real_margin_x + width - real_margin_x);
    _candidateCommentRects[i].OffsetRect(right - _candidateRects[i].right, 0);
    _candidateRects[i].right = right;
  }

  _contentSize.SetSize(width + offsetX * 2, height + offsetY * 2);

  /* Highlighted Candidate */

  _highlightRect = _candidateRects[id];
  // calc page indicator
  if (page_en && candidates_count && !_style.inline_preedit) {
    int _prex = _contentSize.cx - offsetX - real_margin_x +
                _style.hilite_padding_x - pgw;
    int _prey = (_preeditRect.top + _preeditRect.bottom) / 2 - pgszl.cy / 2;
    _prePageRect.SetRect(_prex, _prey, _prex + pgszl.cx, _prey + pgszl.cy);
    _nextPageRect.SetRect(_prePageRect.right + _style.hilite_spacing, _prey,
                          _prePageRect.right + _style.hilite_spacing + pgszr.cx,
                          _prey + pgszr.cy);
    if (ShouldDisplayStatusIcon()) {
      _prePageRect.OffsetRect(-STATUS_ICON_SIZE, 0);
      _nextPageRect.OffsetRect(-STATUS_ICON_SIZE, 0);
    }
  }
  // calc roundings start
  _contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);
  // background rect prepare for Hemispherical calculation
  CopyRect(_bgRect, _contentRect);
  _bgRect.DeflateRect(offsetX + 1, offsetY + 1);
  _PrepareRoundInfo();

  // truely draw content size calculation
  _contentRect.DeflateRect(offsetX, offsetY);
}

#include "HorizontalLayout.h"

using namespace weasel;

void HorizontalLayout::DoLayout() {
  CSize size;
  int width = offsetX + real_margin_x, height = offsetY + real_margin_y;
  int w = offsetX + real_margin_x;

  /* calc mark_text sizes */
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
  int pgw = page_en ? (pgszl.cx + pgszr.cx + _style.hilite_spacing +
                       _style.hilite_padding_x * 2)
                    : 0;
  int pgh = page_en ? MAX(pgszl.cy, pgszr.cy) : 0;

  /* Preedit */
  if (!IsInlinePreedit() && !_context.preedit.str.empty()) {
    size = GetPreeditSize(_context.preedit, _pD2D->pPreeditFormat);
    int szx = pgw, szy = MAX(size.cy, pgh);
    // icon size higher then preedit text
    int yoffset = (STATUS_ICON_SIZE >= szy && ShouldDisplayStatusIcon())
                      ? (STATUS_ICON_SIZE - szy) / 2
                      : 0;
    _preeditRect.SetRect(w, height + yoffset, w + size.cx,
                         height + yoffset + size.cy);
    height += szy + 2 * yoffset + _style.spacing;
    width = MAX(width, real_margin_x * 2 + size.cx + szx);
    if (ShouldDisplayStatusIcon())
      width += STATUS_ICON_SIZE;
  }

  /* Auxiliary */
  if (!_context.aux.str.empty()) {
    size = GetPreeditSize(_context.aux, _pD2D->pPreeditFormat);
    // icon size higher then auxiliary text
    int yoffset = (STATUS_ICON_SIZE >= size.cy && ShouldDisplayStatusIcon())
                      ? (STATUS_ICON_SIZE - size.cy) / 2
                      : 0;
    _auxiliaryRect.SetRect(w, height + yoffset, w + size.cx,
                           height + yoffset + size.cy);
    height += size.cy + 2 * yoffset + _style.spacing;
    width = MAX(width, real_margin_x * 2 + size.cx);
  }

  int row_cnt = 0;
  int max_width_of_rows = 0;
  int height_of_rows[MAX_CANDIDATES_COUNT] = {0};   // height of every row
  int row_of_candidate[MAX_CANDIDATES_COUNT] = {0}; // row info of every cand
  int mintop_of_rows[MAX_CANDIDATES_COUNT] = {0};
  // only when there are candidates
  if (candidates_count) {
    w = offsetX + real_margin_x;
    for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
      int current_cand_width = 0;
      if (i > 0)
        w += _style.candidate_spacing;
      if (id == i)
        w += base_offset;
      /* Label */
      auto &label = labels.at(i).str;
      _pD2D->GetTextSize(label, label.length(), _pD2D->pLabelFormat, &size);
      _candidateLabelRects[i].SetRect(w, height, w + size.cx * labelFontValid,
                                      height + size.cy);
      w += size.cx * labelFontValid;
      current_cand_width += size.cx * labelFontValid;

      /* Text */
      w += _style.hilite_spacing;
      const std::wstring &text = candidates.at(i).str;
      _pD2D->GetTextSize(text, text.length(), _pD2D->pTextFormat, &size);
      _candidateTextRects[i].SetRect(w, height, w + size.cx * textFontValid,
                                     height + size.cy);
      w += size.cx * textFontValid;
      current_cand_width += (size.cx + _style.hilite_spacing) * textFontValid;

      /* Comment */
      bool cmtFontNotTrans =
          (i == id && (_style.hilited_comment_text_color & 0xff000000)) ||
          (i != id && (_style.comment_text_color & 0xff000000));
      if (!comments.at(i).str.empty() && cmtFontValid && cmtFontNotTrans) {
        const std::wstring &comment = comments.at(i).str;
        _pD2D->GetTextSize(comment, comment.length(), _pD2D->pCommentFormat,
                           &size);
        w += _style.hilite_spacing;
        _candidateCommentRects[i].SetRect(w, height, w + size.cx * cmtFontValid,
                                          height + size.cy);
        w += size.cx * cmtFontValid;
        current_cand_width += (size.cx + _style.hilite_spacing) * cmtFontValid;
      } else /* Used for highlighted candidate calculation below */
        _candidateCommentRects[i].SetRect(w, height, w, height + size.cy);

      int base_left = (i == id) ? _candidateLabelRects[i].left - base_offset
                                : _candidateLabelRects[i].left;
      // if not the first candidate of current row, and current candidate's
      // right > _style.max_width
      if (_style.max_width > 0 && (base_left > real_margin_x + offsetX) &&
          (_candidateCommentRects[i].right - offsetX + real_margin_x >
           _style.max_width)) {
        // max_width_of_rows current row
        max_width_of_rows =
            MAX(max_width_of_rows, _candidateCommentRects[i - 1].right);
        w = offsetX + real_margin_x + (i == id ? base_offset : 0);
        int ofx = w - _candidateLabelRects[i].left;
        int ofy = height_of_rows[row_cnt] + _style.candidate_spacing;
        // offset rects to next row
        _candidateLabelRects[i].OffsetRect(ofx, ofy);
        _candidateTextRects[i].OffsetRect(ofx, ofy);
        _candidateCommentRects[i].OffsetRect(ofx, ofy);
        // max width of next row, if it's the last candidate, make sure
        // max_width_of_rows calc right
        max_width_of_rows =
            MAX(max_width_of_rows, _candidateCommentRects[i].right);
        mintop_of_rows[row_cnt] = height;
        height += ofy;
        // re calc rect position, decrease offsetX for origin
        w += current_cand_width;
        row_cnt++;
      } else
        max_width_of_rows = MAX(max_width_of_rows, w);
      // calculate height of current row is the max of three rects
      mintop_of_rows[row_cnt] = height;
      height_of_rows[row_cnt] =
          MAX(height_of_rows[row_cnt], _candidateLabelRects[i].Height());
      height_of_rows[row_cnt] =
          MAX(height_of_rows[row_cnt], _candidateTextRects[i].Height());
      height_of_rows[row_cnt] =
          MAX(height_of_rows[row_cnt], _candidateCommentRects[i].Height());
      // set row info of current candidate
      row_of_candidate[i] = row_cnt;
    }

    // reposition for alignment, exp when rect height not equal to
    // height_of_rows
    for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
      int base_left = (i == id) ? _candidateLabelRects[i].left - base_offset
                                : _candidateLabelRects[i].left;
      _candidateRects[i].SetRect(base_left, mintop_of_rows[row_of_candidate[i]],
                                 _candidateCommentRects[i].right,
                                 mintop_of_rows[row_of_candidate[i]] +
                                     height_of_rows[row_of_candidate[i]]);
      int ol = 0, ot = 0, oc = 0;
      if (_style.align_type == UIStyle::ALIGN_CENTER) {
        ol = (height_of_rows[row_of_candidate[i]] -
              _candidateLabelRects[i].Height()) /
             2;
        ot = (height_of_rows[row_of_candidate[i]] -
              _candidateTextRects[i].Height()) /
             2;
        oc = (height_of_rows[row_of_candidate[i]] -
              _candidateCommentRects[i].Height()) /
             2;
      } else if (_style.align_type == UIStyle::ALIGN_BOTTOM) {
        ol = (height_of_rows[row_of_candidate[i]] -
              _candidateLabelRects[i].Height());
        ot = (height_of_rows[row_of_candidate[i]] -
              _candidateTextRects[i].Height());
        oc = (height_of_rows[row_of_candidate[i]] -
              _candidateCommentRects[i].Height());
      }
      _candidateLabelRects[i].OffsetRect(0, ol);
      _candidateTextRects[i].OffsetRect(0, ot);
      _candidateCommentRects[i].OffsetRect(0, oc);
    }
    height = mintop_of_rows[row_cnt] + height_of_rows[row_cnt] - offsetY;
    width = MAX(width, max_width_of_rows);
  } else {
    height -= _style.spacing + offsetY;
    width += _style.hilite_spacing + _style.border;
  }

  width += real_margin_x;
  height += real_margin_y;

  if (candidates_count) {
    width = MAX(width, _style.min_width);
    height = MAX(height, _style.min_height);
  }
  if (candidates_count) {
    for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
      // make rightest candidate's rect right the same for better look
      if ((i < candidates_count - 1 &&
           row_of_candidate[i] < row_of_candidate[i + 1]) ||
          (i == candidates_count - 1))
        _candidateRects[i].right = width - real_margin_x;
    }
  }
  _highlightRect = _candidateRects[id];
  UpdateStatusIconLayout(&width, &height);
  _contentSize.SetSize(width + offsetX, height + 2 * offsetY);
  _contentRect.SetRect(0, 0, _contentSize.cx, _contentSize.cy);

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

  // prepare temp rect _bgRect for roundinfo calculation
  CopyRect(_bgRect, _contentRect);
  _bgRect.DeflateRect(offsetX + 1, offsetY + 1);
  // prepare round info for single row status, only for single row situation
  _PrepareRoundInfo();
  // readjust for multi rows
  if (row_cnt) // row_cnt > 0, at least 2 candidates
  {
    _roundInfo[0].IsBottomLeftNeedToRound = !_roundInfo[0].Hemispherical;
    _roundInfo[candidates_count - 1].IsTopRightNeedToRound =
        !_roundInfo[candidates_count - 1].Hemispherical;
    for (auto i = 1; i < candidates_count; i++) {
      if (_roundInfo[i].Hemispherical) {
        if (row_of_candidate[i - 1] == row_of_candidate[i] - 1) {
          if (row_of_candidate[i] == row_cnt)
            _roundInfo[i].IsBottomLeftNeedToRound = true;
          if (row_of_candidate[i - 1] == 0)
            _roundInfo[i - 1].IsTopRightNeedToRound =
                _style.inline_preedit || !_roundInfo[i - 1].Hemispherical;
          else
            _roundInfo[i - 1].IsTopRightNeedToRound =
                !_roundInfo[i - 1].Hemispherical;
        }
      }
    }
  }
  // truely draw content size calculation
  _contentRect.DeflateRect(offsetX, offsetY);
}

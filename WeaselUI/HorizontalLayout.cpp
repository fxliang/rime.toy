#include "HorizontalLayout.h"

using namespace weasel;

void HorizontalLayout::DoLayout() {
  CSize size;
  int width = offsetX + real_margin_x, height = offsetY + real_margin_y;
  int w = offsetX + real_margin_x;

  /* calc mark_text sizes */
  int base_offset = CalcMarkMetrics(false);

  // calc page indicator
  int pgw = 0, pgh = 0;
  CalcPageIndicator(false, pgw, pgh);
  CSize pgszl, pgszr;
  bool page_en = (_style.prevpage_color & 0xff000000) &&
                 (_style.nextpage_color & 0xff000000);
  if (!IsInlinePreedit()) {
    _pD2D->GetTextSize(pre, pre.length(), _pD2D->pPreeditFormat, &pgszl);
    _pD2D->GetTextSize(next, next.length(), _pD2D->pPreeditFormat, &pgszr);
  }

  /* Preedit */
  if (!IsInlinePreedit() && !_context.preedit.str.empty()) {
    size = GetPreeditSize(_context.preedit, _pD2D->pPreeditFormat);
    LayoutInlineRect(size, false, true, pgw, pgh, w, width, height,
                     _preeditRect);
  }

  /* Auxiliary */
  if (!_context.aux.str.empty()) {
    size = GetPreeditSize(_context.aux, _pD2D->pPreeditFormat);
    LayoutInlineRect(size, false, false, pgw, pgh, w, width, height,
                     _auxiliaryRect);
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
      height_of_rows[row_cnt] = MAX(
          height_of_rows[row_cnt], _candidateLabelRects[i].Height(),
          _candidateTextRects[i].Height(), _candidateCommentRects[i].Height());
      // set row info of current candidate
      row_of_candidate[i] = row_cnt;
    }

    // reposition for alignment, exp when rect height not equal to
    // height_of_rows
    if (_style.align_type != UIStyle::ALIGN_TOP) {
      for (auto i = 0; i < candidates_count && i < MAX_CANDIDATES_COUNT; ++i) {
        int base_left = (i == id) ? _candidateLabelRects[i].left - base_offset
                                  : _candidateLabelRects[i].left;
        const int height_of_current_raw = height_of_rows[row_of_candidate[i]];
        _candidateRects[i].SetRect(
            base_left, mintop_of_rows[row_of_candidate[i]],
            _candidateCommentRects[i].right,
            mintop_of_rows[row_of_candidate[i]] + height_of_current_raw);
        int ol = 0, ot = 0, oc = 0;
        if (_style.align_type == UIStyle::ALIGN_CENTER) {
          ol = (height_of_current_raw - _candidateLabelRects[i].Height()) / 2;
          ot = (height_of_current_raw - _candidateTextRects[i].Height()) / 2;
          oc = (height_of_current_raw - _candidateCommentRects[i].Height()) / 2;
        } else if (_style.align_type == UIStyle::ALIGN_BOTTOM) {
          ol = (height_of_current_raw - _candidateLabelRects[i].Height());
          ot = (height_of_current_raw - _candidateTextRects[i].Height());
          oc = (height_of_current_raw - _candidateCommentRects[i].Height());
        }
        _candidateLabelRects[i].OffsetRect(0, ol);
        _candidateTextRects[i].OffsetRect(0, ot);
        _candidateCommentRects[i].OffsetRect(0, oc);
      }
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

  // Precompute preedit sub-rectangles
  _PrecomputePreeditRects(_preeditRect, _context.preedit, _preeditBeforeRect,
                          _preeditHiliteRect, _preeditAfterRect);

  // Precompute auxiliary sub-rectangles
  _PrecomputePreeditRects(_auxiliaryRect, _context.aux, _auxBeforeRect,
                          _auxHiliteRect, _auxAfterRect);

  // truely draw content size calculation
  _contentRect.DeflateRect(offsetX, offsetY);
}

#pragma once
#include "d2d.h"
#include <BaseTypes.h>
#include <WeaselIPCData.h>
#include <WeaselUI.h>

namespace weasel {

const int MAX_CANDIDATES_COUNT = 100;
const int STATUS_ICON_SIZE = GetSystemMetrics(SM_CXICON);

#define IS_FULLSCREENLAYOUT(style)                                             \
  (style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN ||                 \
   style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN)
#define NOT_FULLSCREENLAYOUT(style)                                            \
  (style.layout_type != UIStyle::LAYOUT_VERTICAL_FULLSCREEN &&                 \
   style.layout_type != UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN)

class Layout {
public:
  Layout(const UIStyle &style, const Context &context, const Status &status,
         an<D2D> &pD2D);
  virtual void DoLayout() = 0;
  virtual CSize GetContentSize() const = 0;
  virtual CRect GetPreeditRect() const = 0;
  virtual CRect GetAuxiliaryRect() const = 0;
  virtual CRect GetHighlightRect() const = 0;
  virtual CRect GetCandidateLabelRect(int id) const = 0;
  virtual CRect GetCandidateTextRect(int id) const = 0;
  virtual CRect GetCandidateCommentRect(int id) const = 0;
  virtual CRect GetCandidateRect(int id) const = 0;
  virtual CRect GetStatusIconRect() const = 0;
  virtual CRect GetContentRect() const = 0;
  virtual CRect GetPrepageRect() const = 0;
  virtual CRect GetNextpageRect() const = 0;
  virtual CSize GetBeforeSize() const = 0;
  virtual CSize GetHiliteSize() const = 0;
  virtual CSize GetAfterSize() const = 0;
  virtual TextRange GetPreeditRange() const = 0;
  virtual wstring GetLabelText(const vector<Text> &labels, int id,
                               const wchar_t *format) const = 0;
  virtual bool IsInlinePreedit() const = 0;
  virtual bool ShouldDisplayStatusIcon() const = 0;
  virtual IsToRoundStruct GetRoundInfo(int id) = 0;
  virtual IsToRoundStruct GetTextRoundInfo() = 0;

  int offsetX = 0;
  int offsetY = 0;
  int mark_width = 4;
  int mark_gap = 8;
  int mark_height = 0;
  int real_margin_x;
  int real_margin_y;
  UIStyle _style;
  an<D2D> &_pD2D;

protected:
  const Context &_context;
  const Status &_status;
  const vector<Text> &candidates;
  const vector<Text> &labels;
  const vector<Text> &comments;
  const int &id;
  const int candidates_count;
  const int labelFontValid;
  const int textFontValid;
  const int cmtFontValid;
};

} // namespace weasel

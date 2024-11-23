#pragma once
#include "Layout.h"

namespace weasel {

class StandardLayout : public Layout {

public:
  StandardLayout(const UIStyle &style, const Context &context,
                 const Status &status, an<D2D> &pD2D)
      : Layout(style, context, status, pD2D) {}
  virtual void DoLayout() = 0;
  virtual CSize GetContentSize() const { return _contentSize; };
  virtual CRect GetPreeditRect() const { return _preeditRect; };
  virtual CRect GetAuxiliaryRect() const { return _auxiliaryRect; };
  virtual CRect GetHighlightRect() const { return _highlightRect; };
  virtual CRect GetCandidateLabelRect(int id) const {
    return _candidateLabelRects[id];
  };
  virtual CRect GetCandidateTextRect(int id) const {
    return _candidateTextRects[id];
  };
  virtual CRect GetCandidateCommentRect(int id) const {
    return _candidateCommentRects[id];
  };
  virtual CRect GetCandidateRect(int id) const { return _candidateRects[id]; }
  virtual CRect GetStatusIconRect() const { return _statusIconRect; };
  virtual CRect GetContentRect() const { return _contentRect; };
  virtual CRect GetPrepageRect() const { return _prePageRect; };
  virtual CRect GetNextpageRect() const { return _nextPageRect; };
  virtual CSize GetBeforeSize() const { return _beforesz; };
  virtual CSize GetHiliteSize() const { return _hilitedsz; };
  virtual CSize GetAfterSize() const { return _aftersz; };
  virtual TextRange GetPreeditRange() const { return _range; };
  virtual wstring GetLabelText(const vector<Text> &labels, int id,
                               const wchar_t *format) const;
  virtual bool IsInlinePreedit() const;
  virtual bool ShouldDisplayStatusIcon() const;

  virtual IsToRoundStruct GetRoundInfo(int id) { return _roundInfo[id]; };
  virtual IsToRoundStruct GetTextRoundInfo() { return _textRoundInfo; };

protected:
  bool _IsHighlightOverCandidateWindow(const CRect &rc);
  void _PrepareRoundInfo();
  CSize GetPreeditSize(const Text &text,
                       ComPtr<IDWriteTextFormat1> &pTextFormat);
  void UpdateStatusIconLayout(int *width, int *height);

  CSize _beforesz, _hilitedsz, _aftersz;
  TextRange _range;
  CSize _contentSize;
  CRect _preeditRect, _auxiliaryRect, _highlightRect;
  CRect _candidateRects[MAX_CANDIDATES_COUNT];
  CRect _candidateLabelRects[MAX_CANDIDATES_COUNT];
  CRect _candidateTextRects[MAX_CANDIDATES_COUNT];
  CRect _candidateCommentRects[MAX_CANDIDATES_COUNT];
  CRect _statusIconRect;
  CRect _bgRect;
  CRect _contentRect;

  IsToRoundStruct _roundInfo[MAX_CANDIDATES_COUNT];
  IsToRoundStruct _textRoundInfo;

  CRect _prePageRect;
  CRect _nextPageRect;
  const wstring pre = L"<";
  const wstring next = L">";
};
} // namespace weasel

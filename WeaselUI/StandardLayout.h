#pragma once
#include "Layout.h"

namespace weasel {

class StandardLayout : public Layout {

public:
  StandardLayout(const UIStyle &style, const Context &context,
                 const Status &status, an<D2D> &pD2D)
      : Layout(style, context, status, pD2D) {}
  virtual void DoLayout() = 0;
  virtual CSize &GetContentSize() { return _contentSize; };
  virtual CRect &GetPreeditRect() { return _preeditRect; };
  virtual CRect &GetAuxiliaryRect() { return _auxiliaryRect; };
  virtual CRect &GetHighlightRect() { return _highlightRect; };
  virtual CRect &GetCandidateLabelRect(int id) {
    return _candidateLabelRects[id];
  };
  virtual CRect &GetCandidateTextRect(int id) {
    return _candidateTextRects[id];
  };
  virtual CRect &GetCandidateCommentRect(int id) {
    return _candidateCommentRects[id];
  };
  virtual CRect &GetCandidateRect(int id) { return _candidateRects[id]; }
  virtual CRect &GetStatusIconRect() { return _statusIconRect; };
  virtual CRect &GetContentRect() { return _contentRect; };
  virtual CRect &GetPrepageRect() { return _prePageRect; };
  virtual CRect &GetNextpageRect() { return _nextPageRect; };
  virtual const TextRange &GetPreeditRange() const { return _range; };
  virtual bool IsInlinePreedit() const;
  virtual bool ShouldDisplayStatusIcon() const;

  virtual const IsToRoundStruct &GetRoundInfo(int id) {
    return _roundInfo[id];
  };
  virtual const IsToRoundStruct &GetTextRoundInfo() { return _textRoundInfo; };
  // Precomputed preedit sub-rectangles for optimization
  virtual CRect &GetPreeditBeforeRect() { return _preeditBeforeRect; };
  virtual CRect &GetPreeditHiliteRect() { return _preeditHiliteRect; };
  virtual CRect &GetPreeditAfterRect() { return _preeditAfterRect; };
  virtual CRect &GetAuxBeforeRect() { return _auxBeforeRect; };
  virtual CRect &GetAuxHiliteRect() { return _auxHiliteRect; };
  virtual CRect &GetAuxAfterRect() { return _auxAfterRect; };

protected:
  bool _IsHighlightOverCandidateWindow(const CRect &rc);
  void _PrepareRoundInfo();
  CSize GetPreeditSize(const Text &text,
                       ComPtr<IDWriteTextFormat1> &pTextFormat);
  void UpdateStatusIconLayout(int *width, int *height);
  void _PrecomputePreeditRects(const CRect &baseRect, const Text &text,
                               CRect &beforeRect, CRect &hiliteRect,
                               CRect &afterRect);

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
  // Precomputed preedit sub-rectangles for optimization
  CRect _preeditBeforeRect, _preeditHiliteRect, _preeditAfterRect;
  CRect _auxBeforeRect, _auxHiliteRect, _auxAfterRect;

  const wstring pre = L"<";
  const wstring next = L">";
};
} // namespace weasel

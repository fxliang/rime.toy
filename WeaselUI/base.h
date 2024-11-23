#ifndef BASE_H_
#define BASE_H_
#pragma once
#include <algorithm>
#include <windows.h>

class CPoint : public tagPOINT {
public:
  CPoint() { x = 0, y = 0; }
  CPoint(int initX, int initY) { x = initX, y = initY; }
  CPoint(POINT pt) { x = pt.x, y = pt.y; }
  void Offset(int xOffset, int yOffset) { x += xOffset, y += yOffset; }
  void Offset(POINT pt) { x += pt.x, y += pt.y; }
  void Offset(SIZE sz) { x += sz.cx, y += sz.cy; }
  BOOL operator==(POINT pt) const { return x == pt.x && y == pt.y; }
  BOOL operator!=(POINT pt) const { return x != pt.x || y != pt.y; }
  void operator+=(SIZE sz) { x += sz.cx, y += sz.cy; }
  void operator+=(POINT pt) { x += pt.x, y += pt.y; }
  void operator-=(SIZE sz) { x -= sz.cx, y -= sz.cy; }
  void operator-=(POINT pt) { x -= pt.x, y -= pt.y; }
  CPoint operator+(SIZE sz) { return CPoint(x + sz.cx, y + sz.cy); }
  CPoint operator+(POINT pt) { return CPoint(x + pt.x, y + pt.y); }
  CPoint operator-(SIZE sz) { return CPoint(x - sz.cx, y - sz.cy); }
  CPoint operator-(POINT pt) { return CPoint(x - pt.x, y - pt.y); }
};

class CSize : public SIZE {
public:
  CSize() noexcept { cx = 0, cy = 0; }
  CSize(int initCX, int initCY) noexcept { cx = initCX, cy = initCY; }
  CSize(SIZE initSize) noexcept { cx = initSize.cx, cy = initSize.cy; }
  CSize(POINT initPt) noexcept { cx = initPt.x, cy = initPt.y; }
  void SetSize(int x, int y) noexcept { cx = x, cy = y; }
  BOOL operator==(SIZE sz) noexcept { return (cx == sz.cx && cy == sz.cy); }
  BOOL operator!=(SIZE sz) noexcept { return !(*this == sz); }
};

class CRect : public RECT {
public:
  CRect() { left = right = top = bottom = 0; }
  CRect(int l, int t, int r, int b) {
    left = l, right = r, top = t, bottom = b;
    NormalizeRect();
  }
  CRect(const RECT &srcRect) noexcept {
    left = srcRect.left, right = srcRect.right, top = srcRect.top,
    bottom = srcRect.bottom;
    NormalizeRect();
  }

  void SetRect(int x1, int y1, int x2, int y2) noexcept {
    left = x1, right = x2, top = y1, bottom = y2;
    NormalizeRect();
  }
  void InflateRect(int x, int y) noexcept {
    left -= x, right += x, top -= y, bottom += y;
    NormalizeRect();
  }
  void InflateRect(SIZE sz) noexcept { InflateRect(sz.cx, sz.cy); }
  void DeflateRect(int x, int y) noexcept { InflateRect(-x, -y); }
  void DeflateRect(SIZE sz) noexcept { DeflateRect(sz.cx, sz.cy); }
  BOOL IsRectNull() noexcept { return Width() == 0 || Height() == 0; }
  void NormalizeRect() noexcept {
    if (left > right) {
      std::swap(left, right);
    }
    if (top > bottom) {
      std::swap(top, bottom);
    }
  }
  void OffsetRect(int x, int y) {
    left += x, right += x, top += y, bottom += y;
  }
  void OffsetRect(POINT pt) {
    left += pt.x, right += pt.x, top += pt.y, bottom += pt.y;
  }
  void OffsetRect(SIZE sz) {
    left += sz.cx, right += sz.cx, top += sz.cy, bottom += sz.cy;
  }
  void Inflate(int x, int y) { left -= x, right += x, top -= y, bottom += y; }
  int Width() noexcept {
    NormalizeRect();
    return right - left;
  }
  int Height() noexcept {
    NormalizeRect();
    return bottom - top;
  }
  void MoveToXY(int x, int y) {
    NormalizeRect();
    right += (x - left), bottom += (y - top);
    left = x, top = y;
  }
  void MoveToXY(POINT pt) { MoveToXY(pt.x, pt.y); }
  BOOL PtInRect(POINT pt) { return ::PtInRect(this, pt); }
  CPoint TopLeft() noexcept { return CPoint(left, top); }
  CPoint BottomRight() noexcept { return CPoint(right, bottom); }
  CPoint CenterPoint() noexcept {
    return CPoint((LONG)((right - left) / 2) + left,
                  (LONG)((bottom - top) / 2) + top);
  }

  operator LPRECT() { return this; }
  operator LPCRECT() { return this; }
};

#endif

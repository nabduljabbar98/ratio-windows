#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "gdiplus.lib")

namespace UI {
    using namespace Gdiplus;

    inline Color panelBackground(bool lightMode) {
        return lightMode ? Color(255, 247, 247, 247) : Color(255, 15, 15, 15);
    }
    inline Color panelText(bool lightMode) {
        return lightMode ? Color(255, 0, 0, 0) : Color(255, 255, 255, 255);
    }
    inline Color selectionBackground(bool lightMode) {
        return lightMode ? Color(255, 232, 232, 232) : Color(255, 25, 25, 25);
    }
    inline Color createColor() {
        return Color(255, 40, 205, 65);
    }
    inline Color consumeColor() {
        return Color(255, 255, 59, 48);
    }
    inline Color unclassifiedColor() {
        return Color(255, 255, 159, 10);
    }
    inline Color gridColor(bool lightMode) {
        return lightMode ? Color(255, 204, 204, 204) : Color(255, 36, 36, 36);
    }
    inline Color grayColor() {
        return Color(255, 128, 128, 128);
    }

    inline void drawHairline(Graphics& g, REAL x, REAL y, REAL width, REAL height, bool lightMode) {
        SolidBrush brush(gridColor(lightMode));
        g.FillRectangle(&brush, x, y, width, height);
    }

    // Lucide Moon geometry matching main.swift
    inline void drawWebMoon(Graphics& g, const RectF& bounds, const Color& color) {
        GraphicsState state = g.Save();
        REAL midX = bounds.X + bounds.Width / 2.0f;
        REAL midY = bounds.Y + bounds.Height / 2.0f;

        g.TranslateTransform(midX - 7.0f, midY - 7.0f);
        REAL s = 14.0f / 24.0f;
        g.ScaleTransform(s, s);

        GraphicsPath path;
        path.StartFigure();
        path.AddBezier(PointF(20.985f, 12.486f), PointF(20.724f, 17.323f), PointF(16.680f, 21.086f), PointF(11.837f, 20.999f));
        path.AddBezier(PointF(11.837f, 20.999f), PointF(6.993f, 20.913f), PointF(3.087f, 17.007f), PointF(3.000f, 12.163f));
        path.AddBezier(PointF(3.000f, 12.163f), PointF(2.912f, 7.320f), PointF(6.675f, 3.276f), PointF(11.512f, 3.014f));
        path.AddBezier(PointF(11.512f, 3.014f), PointF(11.917f, 2.992f), PointF(12.129f, 3.474f), PointF(11.914f, 3.817f));
        path.AddBezier(PointF(11.914f, 3.817f), PointF(10.433f, 6.186f), PointF(10.784f, 9.264f), PointF(12.759f, 11.240f));
        path.AddBezier(PointF(12.759f, 11.240f), PointF(14.735f, 13.215f), PointF(17.813f, 13.566f), PointF(20.182f, 12.085f));
        path.AddBezier(PointF(20.182f, 12.085f), PointF(20.526f, 11.870f), PointF(21.007f, 12.081f), PointF(20.985f, 12.486f));

        Pen pen(color, 2.0f);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        pen.SetLineJoin(LineJoinRound);
        g.DrawPath(&pen, &path);
        g.Restore(state);
    }

    // History Clock geometry matching main.swift
    inline void drawHistoryClock(Graphics& g, const RectF& bounds, const Color& color) {
        GraphicsState state = g.Save();
        REAL midX = bounds.X + bounds.Width / 2.0f;
        REAL midY = bounds.Y + bounds.Height / 2.0f;

        g.TranslateTransform(midX - 7.0f, midY - 7.0f);
        REAL s = 14.0f / 24.0f;
        g.ScaleTransform(s, s);

        Pen pen(color, 2.0f);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        pen.SetLineJoin(LineJoinRound);

        GraphicsPath path;
        path.StartFigure();
        path.AddBezier(PointF(3.0f, 12.0f), PointF(3.0f, 16.97f), PointF(7.03f, 21.0f), PointF(12.0f, 21.0f));
        path.AddBezier(PointF(12.0f, 21.0f), PointF(16.97f, 21.0f), PointF(21.0f, 16.97f), PointF(21.0f, 12.0f));
        path.AddBezier(PointF(21.0f, 12.0f), PointF(21.0f, 7.03f), PointF(16.97f, 3.0f), PointF(12.0f, 3.0f));
        path.AddBezier(PointF(12.0f, 3.0f), PointF(9.47f, 3.0f), PointF(7.04f, 4.04f), PointF(5.26f, 5.74f));
        path.AddLine(PointF(5.26f, 5.74f), PointF(3.0f, 8.0f));

        // Arrow head
        GraphicsPath arrow;
        arrow.StartFigure();
        arrow.AddLine(PointF(3.0f, 3.0f), PointF(3.0f, 8.0f));
        arrow.AddLine(PointF(3.0f, 8.0f), PointF(8.0f, 8.0f));

        // Hands
        GraphicsPath hands;
        hands.StartFigure();
        hands.AddLine(PointF(12.0f, 7.0f), PointF(12.0f, 12.0f));
        hands.AddLine(PointF(12.0f, 12.0f), PointF(16.0f, 14.0f));

        g.DrawPath(&pen, &path);
        g.DrawPath(&pen, &arrow);
        g.DrawPath(&pen, &hands);
        g.Restore(state);
    }

    // Back Arrow geometry matching main.swift
    inline void drawBackArrow(Graphics& g, const RectF& bounds, const Color& color) {
        REAL midX = bounds.X + bounds.Width / 2.0f;
        REAL midY = bounds.Y + bounds.Height / 2.0f;

        Pen pen(color, 1.8f);
        pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        pen.SetLineJoin(LineJoinRound);

        g.DrawLine(&pen, PointF(midX + 6.0f, midY), PointF(midX - 6.0f, midY));
        g.DrawLine(&pen, PointF(midX - 6.0f, midY), PointF(midX, midY + 6.0f));
        g.DrawLine(&pen, PointF(midX - 6.0f, midY), PointF(midX, midY - 6.0f));
    }

    inline bool isSystemInLightMode() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD val = 0;
            DWORD size = sizeof(val);
            DWORD type = 0;
            LONG res = RegQueryValueExW(hKey, L"AppsUseLightTheme", NULL, &type, (LPBYTE)&val, &size);
            RegCloseKey(hKey);
            if (res == ERROR_SUCCESS) {
                return val != 0;
            }
        }
        return false; // Default to dark mode if registry key not found
    }

    inline void fillRoundedRect(Graphics& g, Brush* brush, const RectF& rect, REAL radius) {
        GraphicsPath path;
        REAL d = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
        path.CloseFigure();
        g.FillPath(brush, &path);
    }

    inline void drawRoundedRect(Graphics& g, Pen* pen, const RectF& rect, REAL radius) {
        GraphicsPath path;
        REAL d = radius * 2.0f;
        path.AddArc(rect.X, rect.Y, d, d, 180, 90);
        path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270, 90);
        path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0, 90);
        path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90, 90);
        path.CloseFigure();
        g.DrawPath(pen, &path);
    }
}

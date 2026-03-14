#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

// ── Interactive curve-editing canvas ─────────────────────────────────────────
// Owns a copy of the curve data on a 200-pt log-freq grid.
// Two edit operations are available:
//   Smoothing — Gaussian average in log-freq space, width set in octaves.
//   Draw mode — click/drag pushes the curve toward the mouse Y with Gaussian falloff.
//
// State model:
//   origDb     — unedited values loaded from TargetCurve (never mutated)
//   smoothedDb — Gaussian smooth of origDb (recomputed when slider changes)
//   deltaDb    — per-point draw adjustments accumulated on top of smoothedDb
//   displayDb  — smoothedDb[i] + deltaDb[i]  (what gets painted and exported)
class CurveEditorCanvas : public juce::Component
{
public:
    CurveEditorCanvas();

    // Load the working copy.  freqs and db must be the same size.
    void loadCurve    (const std::vector<float>& freqs, const std::vector<float>& db);

    // Set smoothing width in octaves (0 = none).  Recomputes smoothedDb immediately.
    void setSmoothing (float octaves);

    // Toggle draw mode (crosshair cursor).
    void setDrawMode  (bool on);
    bool isDrawMode() const { return drawMode; }

    // Discard all edits, restore original loaded curve.
    void resetAll();

    // Retrieve the current result to pass back to the processor.
    const std::vector<float>& getFreqs() const { return origFreqs; }
    std::vector<float>        getDb()    const;

    void paint     (juce::Graphics&)          override;
    void mouseMove (const juce::MouseEvent&)  override;
    void mouseDown (const juce::MouseEvent&)  override;
    void mouseDrag (const juce::MouseEvent&)  override;
    void mouseExit (const juce::MouseEvent&)  override;

private:
    static constexpr float DB_MIN = -30.0f;
    static constexpr float DB_MAX =  15.0f;

    std::vector<float> origFreqs;
    std::vector<float> origDb;
    std::vector<float> smoothedDb;
    std::vector<float> deltaDb;

    float smoothOctaves { 0.0f };
    bool  drawMode      { false };

    float hoverFreq { -1.0f };
    float hoverDb   { -999.0f };

    void  recomputeSmoothed();
    float displayDb (int i) const noexcept { return smoothedDb[i] + deltaDb[i]; }

    float freqToX  (float f,  float w) const noexcept;
    float dbToY    (float db, float h) const noexcept;
    float xToFreq  (float x,  float w) const noexcept;
    float yToDb    (float y,  float h) const noexcept;

    void applyBrush (float brushFreq, float targetDb);
    void drawGrid   (juce::Graphics& g, float w, float h) const;
};

// ── Curve editor panel ────────────────────────────────────────────────────────
class CurveEditorPanel : public juce::Component
{
public:
    // Called on Apply with the edited freq/db arrays.
    std::function<void (const std::vector<float>&, const std::vector<float>&)> onApply;

    // Called on Cancel (or window close) — no data passed.
    std::function<void()> onCancel;

    CurveEditorPanel();

    void loadCurve (const std::vector<float>& freqs, const std::vector<float>& db);

    void resized() override;
    void paint  (juce::Graphics&) override;

    static constexpr int PANEL_W  = 860;
    static constexpr int PANEL_H  = 480;
    static constexpr int CTRL_H   = 80;

private:
    CurveEditorCanvas  canvas;
    juce::Slider       sldSmooth;
    juce::Label        lblSmooth, lblSmoothVal;
    juce::ToggleButton btnDraw   { "Draw" };
    juce::TextButton   btnReset  { "Reset" };
    juce::TextButton   btnApply  { "Apply" };
    juce::TextButton   btnCancel { "Cancel" };
};

// ── Document window wrapper ───────────────────────────────────────────────────
class CurveEditorWindow : public juce::DocumentWindow
{
public:
    explicit CurveEditorWindow (juce::Component* parent);

    void closeButtonPressed() override;
    CurveEditorPanel* getPanel() { return panel.get(); }

private:
    std::unique_ptr<CurveEditorPanel> panel;
};

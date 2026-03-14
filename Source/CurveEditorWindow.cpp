#include "CurveEditorWindow.h"
#include <cmath>
#include <algorithm>

// ═══════════════════════════════════════════════════════════════════════════════
// CurveEditorCanvas
// ═══════════════════════════════════════════════════════════════════════════════

CurveEditorCanvas::CurveEditorCanvas()
{
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void CurveEditorCanvas::loadCurve (const std::vector<float>& freqs,
                                    const std::vector<float>& db)
{
    origFreqs  = freqs;
    origDb     = db;
    smoothedDb = db;
    deltaDb.assign (db.size(), 0.0f);
    smoothOctaves = 0.0f;
    repaint();
}

void CurveEditorCanvas::setSmoothing (float octaves)
{
    smoothOctaves = octaves;
    recomputeSmoothed();
    repaint();
}

void CurveEditorCanvas::setDrawMode (bool on)
{
    drawMode = on;
    setMouseCursor (on ? juce::MouseCursor::CrosshairCursor
                       : juce::MouseCursor::NormalCursor);
}

void CurveEditorCanvas::resetAll()
{
    smoothedDb    = origDb;
    deltaDb.assign (origDb.size(), 0.0f);
    smoothOctaves = 0.0f;
    repaint();
}

std::vector<float> CurveEditorCanvas::getDb() const
{
    std::vector<float> out (origFreqs.size());
    for (int i = 0; i < (int)origFreqs.size(); ++i)
        out[i] = displayDb (i);
    return out;
}

// ── Gaussian smoothing ────────────────────────────────────────────────────────

void CurveEditorCanvas::recomputeSmoothed()
{
    const int n = static_cast<int> (origFreqs.size());
    if (n == 0) return;

    if (smoothOctaves < 0.01f)
    {
        smoothedDb = origDb;
        return;
    }

    const float sigma  = smoothOctaves / 2.0f;
    const float sigma2 = 2.0f * sigma * sigma;

    smoothedDb.resize (n);

    for (int i = 0; i < n; ++i)
    {
        const float logFi = std::log2 (origFreqs[i]);
        float sumW = 0.0f, sumWdb = 0.0f;

        for (int j = 0; j < n; ++j)
        {
            const float d = std::log2 (origFreqs[j]) - logFi;
            const float w = std::exp (-d * d / sigma2);
            sumW   += w;
            sumWdb += w * origDb[j];
        }

        smoothedDb[i] = (sumW > 0.0f) ? sumWdb / sumW : origDb[i];
    }
}

// ── Draw brush ────────────────────────────────────────────────────────────────

void CurveEditorCanvas::applyBrush (float brushFreq, float targetDb)
{
    if (origFreqs.empty()) return;

    const float sigma  = 0.35f;   // ~0.7-octave brush width
    const float sigma2 = 2.0f * sigma * sigma;
    const float logB   = std::log2 (brushFreq);

    for (int i = 0; i < (int)origFreqs.size(); ++i)
    {
        const float d = std::log2 (origFreqs[i]) - logB;
        const float w = std::exp (-d * d / sigma2);

        // Snap this point toward targetDb, weighted by proximity.
        // Uses "direct set" semantics so dragging is instant and predictable.
        const float curr = displayDb (i);
        deltaDb[i] += w * (targetDb - curr);
    }
}

// ── Coordinate helpers ────────────────────────────────────────────────────────

float CurveEditorCanvas::freqToX (float f, float w) const noexcept
{
    if (f <= 0.0f) return 0.0f;
    const float t = std::log10 (f / 20.0f) / std::log10 (20000.0f / 20.0f);
    return t * w;
}

float CurveEditorCanvas::dbToY (float db, float h) const noexcept
{
    const float t = (db - DB_MAX) / (DB_MIN - DB_MAX);
    return t * h;
}

float CurveEditorCanvas::xToFreq (float x, float w) const noexcept
{
    if (w <= 0.0f) return 20.0f;
    return 20.0f * std::pow (10.0f, (x / w) * std::log10 (20000.0f / 20.0f));
}

float CurveEditorCanvas::yToDb (float y, float h) const noexcept
{
    if (h <= 0.0f) return 0.0f;
    return DB_MAX + (y / h) * (DB_MIN - DB_MAX);
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void CurveEditorCanvas::mouseMove (const juce::MouseEvent& e)
{
    hoverFreq = xToFreq ((float)e.x, (float)getWidth());
    hoverDb   = yToDb   ((float)e.y, (float)getHeight());
    repaint();
}

void CurveEditorCanvas::mouseDown (const juce::MouseEvent& e)
{
    if (!drawMode || origFreqs.empty()) return;

    const float freq     = xToFreq ((float)e.x, (float)getWidth());
    const float targetDb = yToDb   ((float)e.y, (float)getHeight());
    applyBrush (freq, juce::jlimit (DB_MIN, DB_MAX, targetDb));
    repaint();
}

void CurveEditorCanvas::mouseDrag (const juce::MouseEvent& e)
{
    hoverFreq = xToFreq ((float)e.x, (float)getWidth());
    hoverDb   = yToDb   ((float)e.y, (float)getHeight());

    if (!drawMode || origFreqs.empty()) { repaint(); return; }

    const float freq     = xToFreq ((float)e.x, (float)getWidth());
    const float targetDb = juce::jlimit (DB_MIN, DB_MAX,
                               yToDb ((float)e.y, (float)getHeight()));
    applyBrush (freq, targetDb);
    repaint();
}

void CurveEditorCanvas::mouseExit (const juce::MouseEvent&)
{
    hoverFreq = -1.0f;
    repaint();
}

// ── Grid ──────────────────────────────────────────────────────────────────────

void CurveEditorCanvas::drawGrid (juce::Graphics& g, float w, float h) const
{
    static const float freqMarks[] = { 20, 50, 100, 200, 500,
                                       1000, 2000, 5000, 10000, 20000 };
    static const char* freqLabels[] = { "20", "50", "100", "200", "500",
                                        "1k", "2k", "5k", "10k", "20k" };

    g.setFont (10.0f);

    // Frequency lines
    g.setColour (juce::Colour (0xFF2A2A4A));
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (freqMarks[i], w);
        g.drawVerticalLine (juce::roundToInt (x), 0.0f, h);
    }

    g.setColour (juce::Colour (0xFF555577));
    for (int i = 0; i < 10; ++i)
    {
        const float x = freqToX (freqMarks[i], w);
        g.drawText (freqLabels[i], (int)x - 16, (int)h - 14, 32, 12,
                    juce::Justification::centred);
    }

    // dB lines
    for (float db = DB_MIN; db <= DB_MAX; db += 6.0f)
    {
        const float y = dbToY (db, h);
        const bool  isZero = (std::abs (db) < 0.1f);

        g.setColour (isZero ? juce::Colour (0xFF3A3A5A)
                            : juce::Colour (0xFF1E1E32));
        g.drawHorizontalLine (juce::roundToInt (y), 0.0f, w);

        g.setColour (juce::Colour (0xFF555577));
        g.drawText (juce::String ((int)db) + " dB", 4, (int)y - 7, 42, 13,
                    juce::Justification::left);
    }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void CurveEditorCanvas::paint (juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    g.fillAll (juce::Colour (0xFF0D0D1A));
    drawGrid (g, w, h);

    if (origFreqs.empty()) return;

    // 0 dB reference line
    g.setColour (juce::Colour (0x44FFFFFF));
    g.drawHorizontalLine (juce::roundToInt (dbToY (0.0f, h)), 0.0f, w);

    // Curve
    juce::Path path;
    bool started = false;
    for (int i = 0; i < (int)origFreqs.size(); ++i)
    {
        const float x = freqToX (origFreqs[i], w);
        const float y = dbToY   (displayDb (i), h);
        if (!started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    g.setColour (juce::Colour (0xFFFFAA00));
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // Hover crosshair + readout
    if (hoverFreq > 0.0f)
    {
        const float hx = freqToX (hoverFreq, w);
        const float hy = dbToY   (hoverDb,   h);

        g.setColour (juce::Colour (0x55FFFFFF));
        g.drawVerticalLine   (juce::roundToInt (hx), 0.0f, h);
        if (drawMode)
            g.drawHorizontalLine (juce::roundToInt (hy), 0.0f, w);

        // Readout box
        juce::String freqStr, dbStr;
        if (hoverFreq < 999.5f)
            freqStr = juce::String ((int)hoverFreq) + " Hz";
        else
            freqStr = juce::String (hoverFreq / 1000.0f, 1) + " kHz";

        if (drawMode)
            dbStr = juce::String (hoverDb, 1) + " dB";
        else
        {
            // Show the curve's actual value at this frequency (nearest point)
            float nearestDb = 0.0f;
            float bestDist  = 1e9f;
            for (int i = 0; i < (int)origFreqs.size(); ++i)
            {
                const float d = std::abs (origFreqs[i] - hoverFreq);
                if (d < bestDist) { bestDist = d; nearestDb = displayDb (i); }
            }
            dbStr = juce::String (nearestDb, 1) + " dB";
        }

        const juce::String label = freqStr + "  " + dbStr;
        g.setColour (juce::Colour (0xAAFFFFFF));
        g.setFont   (11.0f);
        g.drawText  (label, 50, 6, 200, 14, juce::Justification::left);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// CurveEditorPanel
// ═══════════════════════════════════════════════════════════════════════════════

CurveEditorPanel::CurveEditorPanel()
{
    addAndMakeVisible (canvas);

    // Smooth slider
    sldSmooth.setSliderStyle (juce::Slider::LinearHorizontal);
    sldSmooth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sldSmooth.setRange (0.0, 4.0, 0.05);
    sldSmooth.setValue (0.0, juce::dontSendNotification);
    sldSmooth.onValueChange = [this]
    {
        const float oct = (float)sldSmooth.getValue();
        canvas.setSmoothing (oct);
        lblSmoothVal.setText (juce::String (oct, 2) + " oct",
                              juce::dontSendNotification);
    };
    addAndMakeVisible (sldSmooth);

    lblSmooth.setText ("Smooth", juce::dontSendNotification);
    lblSmooth.setFont (11.0f);
    lblSmooth.setJustificationType (juce::Justification::centredRight);
    lblSmooth.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
    addAndMakeVisible (lblSmooth);

    lblSmoothVal.setText ("0.00 oct", juce::dontSendNotification);
    lblSmoothVal.setFont (11.0f);
    lblSmoothVal.setColour (juce::Label::textColourId, juce::Colour (0xFFFFDD88));
    addAndMakeVisible (lblSmoothVal);

    // Draw toggle
    btnDraw.setClickingTogglesState (true);
    btnDraw.setTooltip ("Toggle draw mode — click/drag on the curve to reshape it");
    btnDraw.onClick = [this]
    {
        canvas.setDrawMode (btnDraw.getToggleState());
    };
    addAndMakeVisible (btnDraw);

    // Reset
    btnReset.setTooltip ("Discard all edits and restore the original loaded curve");
    btnReset.onClick = [this]
    {
        sldSmooth.setValue (0.0, juce::dontSendNotification);
        lblSmoothVal.setText ("0.00 oct", juce::dontSendNotification);
        btnDraw.setToggleState (false, juce::sendNotification);
        canvas.resetAll();
    };
    addAndMakeVisible (btnReset);

    // Apply
    btnApply.onClick = [this]
    {
        if (onApply)
            onApply (canvas.getFreqs(), canvas.getDb());
    };
    addAndMakeVisible (btnApply);

    // Cancel
    btnCancel.onClick = [this]
    {
        if (onCancel) onCancel();
    };
    addAndMakeVisible (btnCancel);

    setSize (PANEL_W, PANEL_H);
}

void CurveEditorPanel::loadCurve (const std::vector<float>& freqs,
                                   const std::vector<float>& db)
{
    sldSmooth.setValue (0.0, juce::dontSendNotification);
    lblSmoothVal.setText ("0.00 oct", juce::dontSendNotification);
    btnDraw.setToggleState (false, juce::sendNotification);
    canvas.loadCurve (freqs, db);
}

void CurveEditorPanel::resized()
{
    const int w      = getWidth();
    const int canvasH = getHeight() - CTRL_H;
    canvas.setBounds (0, 0, w, canvasH);

    const int cy = canvasH + (CTRL_H - 28) / 2;   // vertically centered in ctrl strip

    // Left: Smooth label + slider + value
    int x = 12;
    lblSmooth   .setBounds (x,      cy,      55, 20);  x += 58;
    sldSmooth   .setBounds (x,      cy,     180, 20);  x += 184;
    lblSmoothVal.setBounds (x,      cy,      62, 20);  x += 70;

    // Middle: Draw + Reset
    x += 12;
    btnDraw .setBounds (x, cy, 64, 24);  x += 72;
    btnReset.setBounds (x, cy, 64, 24);

    // Right: Cancel + Apply
    btnApply .setBounds (w - 88,  cy, 76, 24);
    btnCancel.setBounds (w - 172, cy, 76, 24);
}

void CurveEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0D0D1A));

    // Control strip background
    const int ctrlY = getHeight() - CTRL_H;
    g.setColour (juce::Colour (0xFF14141F));
    g.fillRect  (0, ctrlY, getWidth(), CTRL_H);

    g.setColour (juce::Colour (0xFF2A2A4A));
    g.drawHorizontalLine (ctrlY, 0.0f, (float)getWidth());

    // Hint text
    g.setColour (juce::Colour (0xFF444466));
    g.setFont   (10.0f);
    g.drawText  ("Draw mode: drag on canvas to reshape  •  Smooth: Gaussian average in log-freq space",
                 12, getHeight() - 18, getWidth() - 24, 14,
                 juce::Justification::left);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CurveEditorWindow
// ═══════════════════════════════════════════════════════════════════════════════

CurveEditorWindow::CurveEditorWindow (juce::Component* parent)
    : juce::DocumentWindow ("Edit Target Curve",
                             juce::Colour (0xFF0D0D1A),
                             juce::DocumentWindow::closeButton)
{
    setUsingNativeTitleBar (true);
    panel = std::make_unique<CurveEditorPanel>();
    setContentNonOwned (panel.get(), true);
    setResizable (false, false);
    centreWithSize (CurveEditorPanel::PANEL_W,
                    CurveEditorPanel::PANEL_H + getTitleBarHeight());
    setVisible (true);

    // Cancel closes the window
    panel->onCancel = [this] { setVisible (false); };
}

void CurveEditorWindow::closeButtonPressed()
{
    setVisible (false);
}

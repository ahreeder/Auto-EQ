#include "BandEditorPanel.h"

BandEditorPanel::BandEditorPanel()
{
    for (int i = 0; i < MAX_BANDS; ++i)
    {
        rows[i].bandIdx = i;
        setupRow (i);
        addAndMakeVisible (rows[i].numLabel);
        addAndMakeVisible (rows[i].freqSlider);
        addAndMakeVisible (rows[i].qSlider);
        addAndMakeVisible (rows[i].gainSlider);
        addAndMakeVisible (rows[i].lockBtn);
    }

    setSize (PANEL_W, panelHeight());
}

void BandEditorPanel::setupRow (int i)
{
    auto& r = rows[i];

    r.numLabel.setText ("B" + juce::String::formatted ("%02d", i + 1),
                        juce::dontSendNotification);
    r.numLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    r.numLabel.setJustificationType (juce::Justification::centred);
    r.numLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));

    // Frequency: 20 Hz - 20 kHz, log-skewed
    r.freqSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    r.freqSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 18);
    r.freqSlider.setRange (20.0, 20000.0, 0.5);
    r.freqSlider.setSkewFactorFromMidPoint (1000.0);
    r.freqSlider.setTextValueSuffix (" Hz");
    r.freqSlider.setValue (1000.0, juce::dontSendNotification);
    r.freqSlider.onValueChange = [this, i] { if (!isUpdating) rowChanged (i); };

    // Q: 0.3 - 10
    r.qSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    r.qSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
    r.qSlider.setRange (0.3, 10.0, 0.05);
    r.qSlider.setValue (1.41, juce::dontSendNotification);
    r.qSlider.onValueChange = [this, i] { if (!isUpdating) rowChanged (i); };

    // Gain: -24 to +24 dB
    r.gainSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    r.gainSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    r.gainSlider.setRange (-24.0, 24.0, 0.1);
    r.gainSlider.setTextValueSuffix (" dB");
    r.gainSlider.setValue (0.0, juce::dontSendNotification);
    r.gainSlider.onValueChange = [this, i] { if (!isUpdating) rowChanged (i); };

    // Lock toggle
    r.lockBtn.setButtonText ("Lock");
    r.lockBtn.setColour (juce::ToggleButton::textColourId,  juce::Colour (0xFF00C8FF));
    r.lockBtn.setColour (juce::ToggleButton::tickColourId,  juce::Colour (0xFF00C8FF));
    r.lockBtn.onStateChange = [this, i] { if (!isUpdating) rowChanged (i); };
}

void BandEditorPanel::rowChanged (int i)
{
    auto& r = rows[i];
    auto  p = currentParams[i];

    p.freq   = static_cast<float> (r.freqSlider.getValue());
    p.q      = static_cast<float> (r.qSlider.getValue());
    p.gainDb = static_cast<float> (r.gainSlider.getValue());
    p.locked = r.lockBtn.getToggleState();

    currentParams[i] = p;

    if (onBandChanged)
        onBandChanged (i, p);
}

void BandEditorPanel::updateBands (const std::array<BandParams, MAX_BANDS>& bands)
{
    isUpdating = true;

    for (int i = 0; i < MAX_BANDS; ++i)
    {
        currentParams[i] = bands[i];
        auto& r = rows[i];

        r.freqSlider.setValue (bands[i].freq,   juce::dontSendNotification);
        r.qSlider   .setValue (bands[i].q,      juce::dontSendNotification);
        r.gainSlider.setValue (bands[i].gainDb,  juce::dontSendNotification);
        r.lockBtn   .setToggleState (bands[i].locked, juce::dontSendNotification);

        const bool active = bands[i].active;
        const juce::Colour textCol = active ? juce::Colour (0xFFDDDDDD)
                                            : juce::Colour (0xFF444455);
        r.numLabel.setColour (juce::Label::textColourId, textCol);
        r.freqSlider.setEnabled (active);
        r.qSlider   .setEnabled (active);
        r.gainSlider.setEnabled (active);
        r.lockBtn   .setEnabled (active);
    }

    isUpdating = false;
}

void BandEditorPanel::resized()
{
    const int y0 = HEADER_H;

    for (int i = 0; i < MAX_BANDS; ++i)
    {
        const int y = y0 + i * ROW_H;
        auto& r = rows[i];

        int x = 4;
        r.numLabel  .setBounds (x, y + 4, 30, ROW_H - 8);  x += 34;
        r.freqSlider.setBounds (x, y + 4, 168, ROW_H - 8); x += 172;
        r.qSlider   .setBounds (x, y + 4, 118, ROW_H - 8); x += 122;
        r.gainSlider.setBounds (x, y + 4, 146, ROW_H - 8); x += 150;
        r.lockBtn   .setBounds (x, y + 4, 54,  ROW_H - 8);
    }
}

void BandEditorPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0D0D1A));

    // Header
    g.setColour (juce::Colour (0xFF14141F));
    g.fillRect (0, 0, getWidth(), HEADER_H);
    g.setColour (juce::Colour (0xFF6666AA));
    g.setFont (10.0f);

    // Column headers
    g.drawText ("Band",      4,   2,  30, 18, juce::Justification::centred);
    g.drawText ("Frequency", 38,  2, 164, 18, juce::Justification::centred);
    g.drawText ("Q",         210, 2, 114, 18, juce::Justification::centred);
    g.drawText ("Gain",      328, 2, 142, 18, juce::Justification::centred);
    g.drawText ("Lock",      474, 2,  54, 18, juce::Justification::centred);

    // Row separators
    g.setColour (juce::Colour (0xFF1E1E30));
    for (int i = 0; i <= MAX_BANDS; ++i)
        g.drawHorizontalLine (HEADER_H + i * ROW_H, 0.0f, (float) getWidth());

    // Highlight active rows
    for (int i = 0; i < MAX_BANDS; ++i)
    {
        if (currentParams[i].active)
        {
            g.setColour (juce::Colour (0xFF1A1A2E));
            g.fillRect (0, HEADER_H + i * ROW_H + 1,
                        getWidth(), ROW_H - 1);
        }
    }
}

#include "PluginEditor.h"

VoxReTuneAudioProcessorEditor::VoxReTuneAudioProcessorEditor(VoxReTuneAudioProcessor& p)
    : juce::AudioProcessorEditor(&p),
      processor(p)
{
    topPanel.setOpaque(true);

    infoBar.setJustificationType(juce::Justification::centredLeft);
    infoBar.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    infoBar.setFont(juce::FontOptions(12.0f));

    addAndMakeVisible(topPanel);
    addAndMakeVisible(pianoRoll);
    addAndMakeVisible(infoBar);

    pianoRoll.setOnHoverNote([this](const juce::String& note)
    {
        infoBar.setText(note, juce::dontSendNotification);
    });

    static constexpr int kMinWidth = 960;
    static constexpr int kMinHeight = 620;

    setResizable(true, true);
    setResizeLimits(kMinWidth, kMinHeight, 4096, 2160);
    setSize(kMinWidth, kMinHeight);
    startTimerHz(30);
}

void VoxReTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101010));

    g.setColour(juce::Colour(0xff181818));
    g.fillRect(topPanel.getBounds());

    g.setColour(juce::Colour(0xff141414));
    g.fillRect(infoBar.getBounds());

    g.setColour(juce::Colour(0xff303030));
    g.drawHorizontalLine(topPanel.getBottom(), 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(infoBar.getY(), 0.0f, static_cast<float>(getWidth()));
}

void VoxReTuneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    topPanel.setBounds(bounds.removeFromTop(120));
    infoBar.setBounds(bounds.removeFromBottom(30));
    pianoRoll.setBounds(bounds);
}

void VoxReTuneAudioProcessorEditor::timerCallback()
{
    const auto transport = processor.getUiTransport();
    pianoRoll.setPitchLine(processor.getDisplayPitchLine());
    pianoRoll.setCorrectionBoxes(processor.getCorrectionBoxes());
    pianoRoll.setTransportState(transport.isPlaying, transport.playheadSeconds);
}

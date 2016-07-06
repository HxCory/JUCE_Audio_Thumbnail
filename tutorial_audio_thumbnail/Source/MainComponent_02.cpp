#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class MainContentComponent   : public AudioAppComponent,
                               public ChangeListener,
                               public ButtonListener,
                               private Timer               // [10]
{
public:
    MainContentComponent()
    :   state (Stopped),
        thumbnailCache (5),
        thumbnail (512, formatManager, thumbnailCache)
    {
        setLookAndFeel (&lookAndFeel);
        
        addAndMakeVisible (&openButton);
        openButton.setButtonText ("Open...");
        openButton.addListener (this);

        addAndMakeVisible (&playButton);
        playButton.setButtonText ("Play");
        playButton.addListener (this);
        playButton.setColour (TextButton::buttonColourId, Colours::green);
        playButton.setEnabled (false);

        addAndMakeVisible (&stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.addListener (this);
        stopButton.setColour (TextButton::buttonColourId, Colours::red);
        stopButton.setEnabled (false);

        setSize (600, 400);

        formatManager.registerBasicFormats();
        transportSource.addChangeListener (this);
        thumbnail.addChangeListener (this);

        setAudioChannels (2, 2);
        startTimer (40);                                  // [11]
    }

    ~MainContentComponent()
    {
        shutdownAudio();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        if (readerSource == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        transportSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        transportSource.releaseResources();
    }

    void paint (Graphics& g) override
    {
        const Rectangle<int> thumbnailBounds (10, 100, getWidth() - 20, getHeight() - 120);

        if (thumbnail.getNumChannels() == 0)
            paintIfNoFileLoaded (g, thumbnailBounds);
        else
            paintIfFileLoaded (g, thumbnailBounds);
    }

    void paintIfNoFileLoaded (Graphics& g, const Rectangle<int>& thumbnailBounds)
    {
        g.setColour (Colours::darkgrey);
        g.fillRect (thumbnailBounds);
        g.setColour (Colours::white);
        g.drawFittedText ("No File Loaded", thumbnailBounds, Justification::centred, 1.0f);
    }

    void paintIfFileLoaded (Graphics& g, const Rectangle<int>& thumbnailBounds)
    {
        g.setColour (Colours::white);
        g.fillRect (thumbnailBounds);

        g.setColour (Colours::red);

        const double audioLength (thumbnail.getTotalLength());                                      // [12]
        thumbnail.drawChannels (g,
                                thumbnailBounds,
                                0.0,
                                audioLength,
                                1.0f);

        g.setColour (Colours::green);

        const double audioPosition (transportSource.getCurrentPosition());
        const float drawPosition ((audioPosition / audioLength) * thumbnailBounds.getWidth()
                                  + thumbnailBounds.getX());                                        // [13]
        g.drawLine (drawPosition, thumbnailBounds.getY(), drawPosition,
                    thumbnailBounds.getBottom(), 2.0f);                                             // [14]

    }

    void resized() override
    {
        openButton.setBounds (10, 10, getWidth() - 20, 20);
        playButton.setBounds (10, 40, getWidth() - 20, 20);
        stopButton.setBounds (10, 70, getWidth() - 20, 20);
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &transportSource) transportSourceChanged();
        else if (source == &thumbnail)  thumbnailChanged();
    }

    void buttonClicked (Button* button) override
    {
        if (button == &openButton)  openButtonClicked();
        if (button == &playButton)  playButtonClicked();
        if (button == &stopButton)  stopButtonClicked();
    }

private:
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Stopping
    };

    void changeState (TransportState newState)
    {
        if (state != newState)
        {
            state = newState;

            switch (state)
            {
                case Stopped:
                    stopButton.setEnabled (false);
                    playButton.setEnabled (true);
                    transportSource.setPosition (0.0);
                    break;

                case Starting:
                    playButton.setEnabled (false);
                    transportSource.start();
                    break;

                case Playing:
                    stopButton.setEnabled (true);
                    break;

                case Stopping:
                    transportSource.stop();
                    break;

                default:
                    jassertfalse;
                    break;
            }
        }
    }

    void transportSourceChanged()
    {
        if (transportSource.isPlaying())
            changeState (Playing);
        else
            changeState (Stopped);
    }

    void thumbnailChanged()
    {
        repaint();
    }

    void openButtonClicked()
    {
        FileChooser chooser ("Select a Wave file to play...",
                             File::nonexistent,
                             "*.wav");

        if (chooser.browseForFileToOpen())
        {
            File file (chooser.getResult());

            if (AudioFormatReader* reader = formatManager.createReaderFor (file))
            {
                ScopedPointer<AudioFormatReaderSource> newSource = new AudioFormatReaderSource (reader, true);
                transportSource.setSource (newSource, 0, nullptr, reader->sampleRate);
                playButton.setEnabled (true);
                thumbnail.setSource (new FileInputSource (file));
                readerSource = newSource.release();
            }
        }
    }

    void playButtonClicked()
    {
        changeState (Starting);
    }

    void stopButtonClicked()
    {
        changeState (Stopping);
    }

    void timerCallback() override
    {
        repaint();
    }

    //==========================================================================
    TextButton openButton;
    TextButton playButton;
    TextButton stopButton;

    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    TransportState state;
    AudioThumbnailCache thumbnailCache;
    AudioThumbnail thumbnail;
    
    LookAndFeel_V3 lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED

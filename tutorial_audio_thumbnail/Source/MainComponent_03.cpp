#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

// example implementation of the Exercise 2

class SimpleThumbnailComponent : public Component,
                                 private ChangeListener
{
public:
    SimpleThumbnailComponent (int sourceSamplesPerThumbnailSample,
                              AudioFormatManager& formatManager,
                              AudioThumbnailCache& cache)
       : thumbnail (sourceSamplesPerThumbnailSample, formatManager, cache)
    {
        thumbnail.addChangeListener (this);
    }

    void setFile (const File& file)
    {
        thumbnail.setSource (new FileInputSource (file));
    }

    void paint (Graphics& g) override
    {
        if (thumbnail.getNumChannels() == 0)
            paintIfNoFileLoaded (g);
        else
            paintIfFileLoaded (g);
    }

    void paintIfNoFileLoaded (Graphics& g)
    {
        g.fillAll (Colours::white);
        g.setColour (Colours::darkgrey);
        g.drawFittedText ("No File Loaded", getLocalBounds(), Justification::centred, 1.0f);
    }

    void paintIfFileLoaded (Graphics& g)
    {
        g.fillAll (Colours::white);

        g.setColour (Colours::red);
        thumbnail.drawChannels (g, getLocalBounds(), 0.0, thumbnail.getTotalLength(), 1.0f);
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &thumbnail)
            thumbnailChanged();
    }

private:
    void thumbnailChanged()
    {
        repaint();
    }

    AudioThumbnail thumbnail;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleThumbnailComponent)
};

//------------------------------------------------------------------------------

class SimplePositionOverlay : public Component,
                              private Timer
{
public:
    SimplePositionOverlay (const AudioTransportSource& transportSourceToUse)
       : transportSource (transportSourceToUse)
    {
        startTimer (40);
    }

    void paint (Graphics& g) override
    {
        const double duration = transportSource.getLengthInSeconds();

        if (duration > 0.0)
        {
            const double audioPosition = transportSource.getCurrentPosition();
            const float drawPosition = (audioPosition / duration) * getWidth();

            g.setColour (Colours::green);
            g.drawLine (drawPosition, 0.0f, drawPosition, (float) getHeight(), 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        repaint();
    }

    const AudioTransportSource& transportSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimplePositionOverlay)
};

//------------------------------------------------------------------------------

class MainContentComponent   : public AudioAppComponent,
                               public ChangeListener,
                               public ButtonListener
{
public:
    MainContentComponent()
      : state (Stopped),
        thumbnailCache (5),
        thumbnailComp (512, formatManager, thumbnailCache),
        positionOverlay (transportSource)
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

        addAndMakeVisible (&thumbnailComp);
        addAndMakeVisible (&positionOverlay);

        setSize (600, 400);

        formatManager.registerBasicFormats();
        transportSource.addChangeListener (this);

        setAudioChannels (2, 2);
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

    void resized() override
    {
        openButton.setBounds (10, 10, getWidth() - 20, 20);
        playButton.setBounds (10, 40, getWidth() - 20, 20);
        stopButton.setBounds (10, 70, getWidth() - 20, 20);

        const Rectangle<int> thumbnailBounds (10, 100, getWidth() - 20, getHeight() - 120);
        thumbnailComp.setBounds (thumbnailBounds);
        positionOverlay.setBounds (thumbnailBounds);
    }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == &transportSource)
            transportSourceChanged();
    }

    void buttonClicked (Button* button) override
    {
        if (button == &openButton)   openButtonClicked();
        if (button == &playButton)   playButtonClicked();
        if (button == &stopButton)   stopButtonClicked();
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
                thumbnailComp.setFile (file);
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

    //==========================================================================
    TextButton openButton;
    TextButton playButton;
    TextButton stopButton;

    AudioFormatManager formatManager;
    ScopedPointer<AudioFormatReaderSource> readerSource;
    AudioTransportSource transportSource;
    TransportState state;
    AudioThumbnailCache thumbnailCache;
    SimpleThumbnailComponent thumbnailComp;
    SimplePositionOverlay positionOverlay;
    
    LookAndFeel_V3 lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};


Component* createMainContentComponent()     { return new MainContentComponent(); }


#endif  // MAINCOMPONENT_H_INCLUDED

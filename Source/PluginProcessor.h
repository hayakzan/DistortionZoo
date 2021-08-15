/*
 ==============================================================================
 
 This code is based on the Distortion module from Juan Gil's Audio Effects project at
 https://github.com/juandagilc/Audio-Effects
 
 as well as the code that accompanies the book "Audio Effects: Theory,
 Implementation and Application" by Joshua D. Reiss and Andrew P. McPherson.
 https://code.soundsoftware.ac.uk/projects/audio_effects_textbook_code/repository
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program. If not, see <https://www.gnu.org/licenses/>.
 
 ==============================================================================
 */


#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginParameter.h"


//==============================================================================

class DistortionAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    DistortionAudioProcessor();
    ~DistortionAudioProcessor();

    //==============================================================================

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (AudioSampleBuffer&, MidiBuffer&) override;

    //==============================================================================
    
    //for ringmod
    float freq = 100.f;
    float amp = 0.5f;


    //==============================================================================

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    //==============================================================================

    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds() const override;

    //==============================================================================

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================

    foleys::LevelMeterSource& getMeterSource()
    {
        return meterSource;
    }


    //==============================================================================

    StringArray distortionTypeItemsUI = {
        "Hard clipping",
        "Soft clipping",
        "Exponential",
        "Full-wave rectifier",
        "Half-wave rectifier",
        "Fold-Back",
        "Squarer",
        "Chebyshev 4th order",
        "Bit crusher",
        "Slew Limiter"
    };

    enum distortionTypeIndex {
        distortionTypeHardClipping = 0,
        distortionTypeSoftClipping,
        distortionTypeExponential,
        distortionTypeFullWaveRectifier,
        distortionTypeHalfWaveRectifier,
        distortionTypeFoldBack,
        distortionTypeSquarer,
        distortionTypeChebT4,
        distortionTypeBitCrusher,
        distortionTypeSlewLimiter,
    };

    //======================================

    class Filter : public IIRFilter 
    {
    public:
        void updateCoefficients (const double discreteFrequency,
                                 const double gain) noexcept
        {
            jassert (discreteFrequency > 0);

            double tan_half_wc = tan (discreteFrequency / 2.0); //wc = cutoff freq
            double sqrt_gain = sqrt (gain);

            coefficients = IIRCoefficients (/* b0 */ sqrt_gain * tan_half_wc + gain,
                                            /* b1 */ sqrt_gain * tan_half_wc - gain,
                                            /* b2 */ 0.0,
                                            /* a0 */ sqrt_gain * tan_half_wc + 1.0,
                                            /* a1 */ sqrt_gain * tan_half_wc - 1.0,
                                            /* a2 */ 0.0);

            setCoefficients (coefficients);
        }
    };

    OwnedArray<Filter> filters;
    void updateFilters();

    //======================================

    PluginParametersManager parameters;

    PluginParameterComboBox paramDistortionType;
    PluginParameterLinSlider paramInputGain;
    PluginParameterLinSlider paramOutputGain;
    PluginParameterLinSlider paramTone;

private:
    foleys::LevelMeterSource meterSource;

    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DistortionAudioProcessor)
};

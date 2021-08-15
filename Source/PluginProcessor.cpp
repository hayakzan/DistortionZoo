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

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameter.h"


//==============================================================================

DistortionAudioProcessor::DistortionAudioProcessor():
#ifndef JucePlugin_PreferredChannelConfigurations
    AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput  ("Input",  AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", AudioChannelSet::stereo(), true)
                    #endif
                   ),
#endif
    parameters (*this)
    , paramDistortionType (parameters, "Distortion type", distortionTypeItemsUI, distortionTypeFullWaveRectifier)
    , paramInputGain (parameters, "Input gain", "dB", -24.0f, 12.0f, 0.0f,
                      [](float value){ return powf (10.0f, value * 0.05f); })
    , paramOutputGain (parameters, "Output gain", "dB", -24.0f, 12.0f, -24.0f,
                       [](float value){ return powf (10.0f, value * 0.05f); })
    , paramTone (parameters, "Tone", "dB", -24.0f, 12.0f, 0.0f,
                 [this](float value){ paramTone.setCurrentAndTargetValue (value); updateFilters(); return value; })
{
    parameters.apvts.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

DistortionAudioProcessor::~DistortionAudioProcessor()
{
}

//==============================================================================

void DistortionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramDistortionType.reset (sampleRate, smoothTime);
    paramInputGain.reset (sampleRate, smoothTime);
    paramOutputGain.reset (sampleRate, smoothTime);
    paramTone.reset (sampleRate, smoothTime);

    //======================================

    filters.clear();
    for (int i = 0; i < getTotalNumInputChannels(); ++i) {
        Filter* filter;
        filters.add (filter = new Filter());
    }
    updateFilters();
    
    //======================================
    
}

void DistortionAudioProcessor::releaseResources()
{
}


void DistortionAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;
    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    
    //======================================

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);

        float out;
        
        //bitcrusher params
        auto undFac = 4; //user param
        auto cpt = 0; //constant
        //slew limiter params
        auto rise = 0.5f; //user param
        auto fall = 0.5f; //user param
        auto slewMin = 0.1f; //constant -max slope in volts per sec
        auto slewMax = 10000.f; //constant -max slope in volts per sec
        auto Ts = 1.f / 48000.f; //replace with actual sample rate
        auto slewRise = slewMax * Ts * powf(slewMin / slewMax, rise);
        auto slewFall = slewMax * Ts * powf(slewMin / slewMax, fall);
        //ring modulator params
//        auto twoPi = 2.f * M_PI;
    
        
        for (int sample = 0; sample < numSamples; ++sample) {
            const float in = channelData[sample] * paramInputGain.getNextValue();
            
            switch ((int)paramDistortionType.getTargetValue()) {
                    
                //hard clipping
                case distortionTypeHardClipping: {
                    float threshold = 0.5f;
                    if (in > threshold)
                        out = threshold;
                    else if (in < -threshold)
                        out = -threshold;
                    else
                        out = in;
                    out *= 0.5f;
                    break;
                }
                    
                //soft clipping
                case distortionTypeSoftClipping: {
                    float threshold1 = 1.0f / 3.0f;
                    float threshold2 = 2.0f / 3.0f;
                    if (in > threshold2)
                        out = 1.0f;
                    else if (in > threshold1)
                        out = 1.0f - powf (2.0f - 3.0f * in, 2.0f) / 3.0f;
                    else if (in < -threshold2)
                        out = -1.0f;
                    else if (in < -threshold1)
                        out = -1.0f + powf (2.0f + 3.0f * in, 2.0f) / 3.0f;
                    else
                        out = 2.0f * in;
                    out *= 0.5f;
                    break;
                }
                    
                //exponential
                case distortionTypeExponential: {
                    if (in > 0.0f)
                        out = 1.0f - expf (-in);
                    else
                        out = -1.0f + expf (in);
                    out *= 0.05f;
                    break;
                }
                
                //full-wave rectifier
                case distortionTypeFullWaveRectifier: {
                    out = fabsf (in);
                    break;
                }
                    
                //half-wave rectifier
                case distortionTypeHalfWaveRectifier: {
                    if (in > 0.0f)
                        out = in;
                    else
                        out = 0.0f;
                    break;
                }
                 
                //fold-back
                case distortionTypeFoldBack: {
                    float fblevel1 = 0.3f;
                    float fblevel2 = fblevel1 * 2.0f;
                    if (in > fblevel1)
                        out = (fblevel2 - out) * 0.05f;
                    else if (in < (-fblevel1)) {
                        out = (-fblevel2 - out) * 0.05f;
                    }
                    break;
                }
   
                //squarer
                case distortionTypeSquarer: {
                    out = powf(in, 2.0f);
                    break;
                }
                    
                //chebyshev 4th order
                case distortionTypeChebT4: {
                    out = (8.0f * powf(in, 4.0f)) - (8.0f * powf(in, 2.0f)) - 1.0f;
                    out = out * 0.1f;
                    break;
                }
                    
                //bit crusher
                case distortionTypeBitCrusher: {
                    if (cpt == 0.0f)
                        out = in;
                    else
                        out = 0.0f;
                    cpt = (cpt + 1) % undFac;
                    break;
                }
                    
                //slew limiter
                case distortionTypeSlewLimiter: {
                    if (in > out)
                        out = jmin (in, out + slewRise);
                    else
                        out = jmax (in, out - slewFall);
                    break;
                }
            }

            float filtered = filters[channel]->processSingleSampleRaw (out);
            channelData[sample] = filtered * paramOutputGain.getNextValue();
        }
    }

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
    
    meterSource.measureBlock (buffer);

}

//==============================================================================

void DistortionAudioProcessor::updateFilters()
{
    double discreteFrequency = M_PI * 0.01;
    //10^paramToneValue * 0.05
    double gain = pow (10.0, (double)paramTone.getTargetValue() * 0.05);

    for (int i = 0; i < filters.size(); ++i)
        filters[i]->updateCoefficients (discreteFrequency, gain);
}

//==============================================================================






//==============================================================================

void DistortionAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DistortionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.apvts.state.getType()))
            parameters.apvts.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* DistortionAudioProcessor::createEditor()
{
    return new DistortionAudioProcessorEditor (*this);
}

bool DistortionAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool DistortionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//==============================================================================

const String DistortionAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DistortionAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DistortionAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DistortionAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DistortionAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int DistortionAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DistortionAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DistortionAudioProcessor::setCurrentProgram (int index)
{
}

const String DistortionAudioProcessor::getProgramName (int index)
{
    return {};
}

void DistortionAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DistortionAudioProcessor();
}

//==============================================================================

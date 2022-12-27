#include "daisysp.h"
#include "daisy_petal.h"
#include "terrarium.h"

// Set max delay time to 0.75 of samplerate.
#define MAX_DELAY static_cast<size_t>(48000 * 2.5f)

using namespace daisysp;
using namespace daisy;
using namespace terrarium;

static DaisyPetal petal;

static ReverbSc                                  rev;
static DelayLine<float, MAX_DELAY> DSY_SDRAM_BSS del;

// Knob parameters
// Delay
static Parameter delayTime, delayFeedback, delayDryWet;
// Reverb
static Parameter reverbFrequency, reverbTime, reverbDryWet;

float currentDelay, feedback, delayTarget, cutoff, dryWet[2];
float sample_rate;
bool  effectOn[2];

Led led1, led2;

//Helper functions
void Controls();
void UpdateLeds();
void InitKnobs();

void GetDelaySample(float &in);
void GetReverbSample(float &in);

enum effectTypes
{
    DEL,
    REV,
};

void AudioCallback(AudioHandle::InterleavingInputBuffer  in,
                   AudioHandle::InterleavingOutputBuffer out,
                   size_t                                size)
{
    Controls();

    float revDryWet = reverbDryWet.Process();
    float delDryWet = delayDryWet.Process();

    //audio
    for(size_t i = 0; i < size; i += 2)
    {
        float sig = in[i];
        float oldSig = sig;

        // Process delay
        if(effectOn[DEL])
        {
            GetDelaySample(sig);
        }
        sig = sig * delDryWet + oldSig * (1 - delDryWet);

        // Process reverb
        float verb = sig * revDryWet;
        GetReverbSample(verb);

        out[i] = sig;

        if(effectOn[REV])
        {
            out[i] += verb;
        }

        // make right signal identical to left since we're not using stereo
        out[i + 1] = out[i];
    }
}

int main(void)
{
    // initialize petal hardware and oscillator daisysp module

    //Inits and sample rate
    petal.Init();
    petal.SetAudioBlockSize(4);
    sample_rate = petal.AudioSampleRate();

    rev.Init(sample_rate);
    del.Init();

    // Init leds
    led1.Init(petal.seed.GetPin(Terrarium::LED_1),false);
    led2.Init(petal.seed.GetPin(Terrarium::LED_2),false);

    InitKnobs();

    //reverb parameters
    rev.SetLpFreq(18000.0f);
    rev.SetFeedback(0.85f);

    //delay parameters
    currentDelay = delayTarget = sample_rate * 0.75f;
    del.SetDelay(currentDelay);

    effectOn[0] = effectOn[1] = false;

    // start callback
    petal.StartAdc();
    petal.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(6);
        UpdateLeds();
    }
}

void InitKnobs()
{
    reverbFrequency.Init(
        petal.knob[Terrarium::KNOB_1], 400, 22000, Parameter::LOGARITHMIC);
    reverbTime.Init(
        petal.knob[Terrarium::KNOB_2], 0.0, 1.0, Parameter::LINEAR);
    reverbDryWet.Init(
        petal.knob[Terrarium::KNOB_3], 0.0, 1.0, Parameter::LINEAR);

    delayTime.Init(
        petal.knob[Terrarium::KNOB_4], sample_rate * .05, MAX_DELAY, delayTime.LOGARITHMIC);
    delayFeedback.Init(
        petal.knob[Terrarium::KNOB_5], 0.0, 1.0, Parameter::LINEAR);
    delayDryWet.Init(
        petal.knob[Terrarium::KNOB_6], 0.0, 1.0, Parameter::LINEAR);
}

void UpdateKnobs()
{
    rev.SetLpFreq(reverbFrequency.Process());
    rev.SetFeedback(reverbTime.Process());

    delayTarget = delayTime.Process();
    feedback = delayFeedback.Process();
}

void UpdateLeds()
{
    //footswitch leds
    led1.Set(effectOn[REV]);
    led2.Set(effectOn[DEL]);
    led1.Update();
    led2.Update();
}

void UpdateSwitches()
{
    //turn the effect on or off if a footswitch is pressed
    effectOn[REV]
        = petal.switches[Terrarium::FOOTSWITCH_1].RisingEdge() ? !effectOn[REV] : effectOn[REV];
    effectOn[DEL]
        = petal.switches[Terrarium::FOOTSWITCH_2].RisingEdge() ? !effectOn[DEL] : effectOn[DEL];
}

void Controls()
{
    petal.ProcessAnalogControls();
    petal.ProcessDigitalControls();

    UpdateKnobs();

    UpdateSwitches();

    UpdateLeds();
}

void GetReverbSample(float &in)
{
    // Pass in to both left/right since we're not using stereo
    rev.Process(in, in, &in, &in);
}

void GetDelaySample(float &in)
{
    fonepole(currentDelay, delayTarget, .00007f);
    del.SetDelay(currentDelay);
    float out = del.Read();

    del.Write((feedback * out) + in);
    in = (feedback * out) + ((1.0f - feedback) * in);
}

#include "daisy_pod.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

DaisyPod   hw;
Oscillator lfo;

// BPM range: 30 - 240
#define TTEMPO_MIN 30
#define TTEMPO_MAX 240

static uint32_t prev_ms = 0;
static float    lfo_out;
static uint16_t tt_count = 0;

// Prototypes
float    bpm_to_freq(uint32_t tempo);
uint32_t ms_to_bpm(uint32_t ms);
void     HandleSystemRealTime(uint8_t srt_type);
void     AudioCallback(AudioHandle::InputBuffer  in,
                       AudioHandle::OutputBuffer out,
                       size_t                    size);

float bpm_to_freq(uint32_t tempo)
{
    return tempo / 60.0f;
}

uint32_t ms_to_bpm(uint32_t ms)
{
    return 60000 / ms;
}

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    hw.ProcessAllControls();

    if(hw.button1.RisingEdge())
    {
        uint32_t ms   = System::GetNow();
        uint32_t diff = ms - prev_ms;
        uint32_t bpm  = ms_to_bpm(diff);
#ifdef DEBUG
        hw.seed.PrintLine("msec=%d, diff=%d, BPM=%d", ms, diff, bpm);
#endif
        if(bpm >= TTEMPO_MIN && bpm <= TTEMPO_MAX)
        {
            // set BPM
            lfo.SetFreq(bpm_to_freq(bpm));
        }

        prev_ms = ms;
    }

    for(size_t i = 0; i < size; i++)
    {
        lfo_out   = lfo.Process();
        out[0][i] = lfo_out * in[0][i];
        out[1][i] = lfo_out * in[1][i];
    }
}

void HandleSystemRealTime(uint8_t srt_type)
{
#ifdef DEBUG
    default: hw.seed.PrintLine("MIDI SystemRealTime: %x", m.srt_type); break;
#endif

        // MIDI Clock -  24 clicks per quarter note
        if(srt_type == TimingClock)
        {
            tt_count++;
            if(tt_count == 24)
            {
                uint32_t ms   = System::GetNow();
                uint32_t diff = ms - prev_ms;
                uint32_t bpm  = ms_to_bpm(diff);
#ifdef DEBUG
                hw.seed.PrintLine("msec=%d, diff=%d, BPM=%d", ms, diff, bpm);
#endif
                if(bpm >= TTEMPO_MIN && bpm <= TTEMPO_MAX)
                {
                    lfo.SetFreq(bpm_to_freq(bpm));
                }

                prev_ms  = ms;
                tt_count = 0;
            }
        }
}

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(4); // number of samples handled per callback
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

    // LFO Settings
    lfo.Init(hw.AudioSampleRate());
    lfo.SetWaveform(lfo.WAVE_SIN);
    lfo.SetFreq(1.0f);
    lfo.SetAmp(1.0f);
#ifdef DEBUG
    hw.seed.StartLog(false);
    System::Delay(250);
#endif
    hw.StartAdc();
    hw.StartAudio(AudioCallback);
    hw.midi.StartReceive();

    while(true)
    {
        hw.midi.Listen();
        while(hw.midi.HasEvents())
        {
            MidiEvent m = hw.midi.PopEvent();
            if(m.type == SystemRealTime)
            {
                HandleSystemRealTime(m.srt_type);
            }
        }
    }
}

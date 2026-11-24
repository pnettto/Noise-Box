// Offline render: runs a WAV file through the exact Stoner Box DSP so results can be measured.
// Usage: StonerBoxRender in.wav out.wav [rig fuzz tone mids doom wall space outputDb]
//        StonerBoxRender in.wav out.wav preset:N
//        StonerBoxRender --presets          (prints one factory preset name per line)
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Presets.h"

int main (int argc, char* argv[])
{
    if (argc == 2 && juce::String (argv[1]) == "--presets")
    {
        for (const auto& preset : getFactoryPresets())
            std::cout << preset.name << "\n";
        return 0;
    }

    if (argc < 3)
    {
        std::cerr << "usage: StonerBoxRender in.wav out.wav [rig fuzz tone mids doom wall space outputDb]\n";
        return 1;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr) { std::cerr << "cannot read input\n"; return 1; }

    StonerBoxParams p;
    float* fields[] = { &p.fuzz, &p.tone, &p.mids, &p.doom, &p.wall, &p.space, &p.outputDb };
    if (argc > 3 && juce::String (argv[3]).startsWith ("preset:"))
    {
        const int idx = juce::String (argv[3]).fromFirstOccurrenceOf (":", false, false).getIntValue();
        p = getFactoryPresets().at ((size_t) idx).params;
    }
    else
    {
        if (argc > 3) p.rig = std::atoi (argv[3]);
        for (int i = 0; i < 7 && 4 + i < argc; ++i)
            *fields[i] = (float) std::atof (argv[4 + i]);
    }

    const int n = (int) reader->lengthInSamples;
    const int tail = (int) reader->sampleRate * 4;
    juce::AudioBuffer<float> in ((int) reader->numChannels, n);
    reader->read (&in, 0, n, 0, true, true);

    juce::AudioBuffer<float> buf (2, n + tail);
    buf.clear();
    buf.copyFrom (0, 0, in, 0, 0, n);
    buf.copyFrom (1, 0, in, in.getNumChannels() > 1 ? 1 : 0, 0, n);

    const int block = 512;
    StonerBoxDSP dsp;
    dsp.setParams (p);
    dsp.prepare (reader->sampleRate, block);

    for (int pos = 0; pos < buf.getNumSamples(); pos += block)
    {
        const int len = juce::jmin (block, buf.getNumSamples() - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        dsp.process (view);
    }

    juce::File out (argv[2]);
    out.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (out.createOutputStream());
    auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions().withSampleRate (reader->sampleRate)
                                                   .withNumChannels (2).withBitsPerSample (24));
    if (writer == nullptr) { std::cerr << "cannot write output\n"; return 1; }
    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    std::cout << "rendered " << out.getFullPathName() << "\n";
    return 0;
}

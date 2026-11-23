// Offline render: runs a WAV file through the exact Jazz Box DSP so results can be measured.
// Usage: JazzBoxRender in.wav out.wav [warmth wood dark squeeze room outputDb]
//        JazzBoxRender in.wav out.wav preset:N
//        JazzBoxRender --presets          (prints one factory preset name per line)
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
        std::cerr << "usage: JazzBoxRender in.wav out.wav [warmth wood dark squeeze room outputDb]\n";
        return 1;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (juce::File (argv[1])));
    if (reader == nullptr) { std::cerr << "cannot read input\n"; return 1; }

    JazzBoxParams p;
    float* fields[] = { &p.warmth, &p.wood, &p.dark, &p.squeeze, &p.room, &p.outputDb };
    if (argc > 3 && juce::String (argv[3]).startsWith ("preset:"))
    {
        const int idx = juce::String (argv[3]).fromFirstOccurrenceOf (":", false, false).getIntValue();
        p = getFactoryPresets().at ((size_t) idx).params;
    }
    else
    {
        for (int i = 0; i < 6 && 3 + i < argc; ++i)
            *fields[i] = (float) std::atof (argv[3 + i]);
    }

    const int n = (int) reader->lengthInSamples;
    const int tail = (int) reader->sampleRate * 2;
    juce::AudioBuffer<float> in ((int) reader->numChannels, n);
    reader->read (&in, 0, n, 0, true, true);

    juce::AudioBuffer<float> buf (2, n + tail);
    buf.clear();
    buf.copyFrom (0, 0, in, 0, 0, n);
    buf.copyFrom (1, 0, in, in.getNumChannels() > 1 ? 1 : 0, 0, n);

    const int block = 512;
    JazzBoxDSP dsp;
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
    auto stream = std::unique_ptr<juce::OutputStream> (out.createOutputStream());
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (
        wav.createWriterFor (stream.get(), reader->sampleRate, 2, 24, {}, 0));
    if (writer == nullptr) { std::cerr << "cannot write output\n"; return 1; }
    stream.release();
    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    std::cout << "rendered " << out.getFullPathName() << "\n";
    return 0;
}

#include "../src/serializer.hpp"
#include "../src/log.hpp"
#include "../src/music_format.hpp"
#include "../miniz/miniz.h"
#include <pugixml.hpp>
#include <cstdlib>

using namespace rana;

double val_double(pugi::xml_node node, const char *name, double fallback)
{
    try {
        return std::stod(node.child_value(name));
    }
    catch (const std::invalid_argument &e) {
        log::warn("could not parse {}, using fallback value {}", name, fallback);
        return fallback;
    }
}

int val_int(pugi::xml_node node, const char *name, int fallback)
{
    try {
        return std::stoi(node.child_value(name));
    }
    catch (const std::invalid_argument &e) {
        log::warn("could not parse {}, using fallback value {}", name, fallback);
        return fallback;
    }
}

std::string val(pugi::xml_node node, const char *name)
{
    return node.child_value(name);
}

int main(int argc, char **argv)
{
    if (argc == 0) {
        return -1;
    }

    if (argc < 3) {
        log::info("usage: corn <input.xrns> <output.ranamus>");
        return -1;
    }

    char *infile = argv[1];
    char *outfile = argv[2];

    log::info("working on file '{}'", infile);

    size_t xml_len;
    char *xml = (char *)mz_zip_extract_archive_file_to_heap(infile, "Song.xml", &xml_len, 0);

    if (xml == nullptr) {
        log::err("failed to read Song.xml from the archive");
        return -1;
    }

    pugi::xml_document doc;
    auto result = doc.load_buffer(xml, xml_len);
    if (result) {
        log::info("parsing xml OK");
    } else {
        log::err("failed to parse xml: {}", result.description());
        return -1;
    }

    auto rnsong = doc.child("RenoiseSong");
    auto sd = rnsong.child("GlobalSongData");

    auto song = musfmt::Song();
    song.bpm = val_double(sd, "BeatsPerMin", 140);
    song.beat_lines = val_int(sd, "LinesPerBeat", 4);
    song.line_ticks = val_int(sd, "TicksPerLine", 12);

    size_t ins_count = 0;
    for (auto ins : rnsong.child("Instruments").children("Instrument")) {
        auto name = val(ins, "Name");

        log::info("parsing instrument {}: '{}'", ins_count, name);
        ins_count++;

        struct musfmt::Instrument instrument = {};

        for (auto smp : ins.child("SampleGenerator").child("Samples").children("Sample")) {
            auto name = val(smp, "Name");

            musfmt::Sample sample = {};
            sample.volume = val_double(smp, "Volume", 1.0);
            sample.pan =    val_double(smp, "Panning", 0.5);
            sample.transpose = val_int(smp, "Transpose", 0);
            sample.fine =      val_int(smp, "Finetune", 0);

            log::info("  sample: '{}'", name);
            log::info("    volume: {}", sample.volume);
            log::info("    pan: {}", sample.pan);
            log::info("    transpose: {}", sample.transpose);
            log::info("    fine: {}", sample.fine);

            instrument.smp.push_back(sample);
        }

        song.ins.push_back(instrument);
    }

    auto ser = Serializer();
    song.serialize(ser);

    log::info("writing output to '{}'...", outfile);
    auto f = std::fopen(outfile, "wb");
    auto bytes = std::fwrite(ser.data().data(), 1, ser.data().size(), f);
    log::info("wrote {} bytes.", bytes);
    fclose(f);

    std::free(xml);
    return 0;
}
#include "serializer.hpp"
#include "log.hpp"
#include "music/format.hpp"
#include "fio.hpp"
#include "miniz/miniz.h"
#include <pugixml.hpp>
#include <cstdlib>
#include <regex>
#include <cmath>

using namespace rana;

double dB(double volume)
{
    return 20 * std::log10(volume);
}

double val_double(pugi::xml_node node, const char *name, double fallback)
{
    try {
        return std::stod(node.child_value(name));
    }
    catch (const std::invalid_argument &e) {
        log::warn("could not parse %s, using fallback value %f", name, fallback);
        return fallback;
    }
}

int val_int(pugi::xml_node node, const char *name, int fallback)
{
    try {
        return std::stoi(node.child_value(name));
    }
    catch (const std::invalid_argument &e) {
        log::warn("could not parse %s, using fallback value %d", name, fallback);
        return fallback;
    }
}

int val_hex(pugi::xml_node node, const char *name, int fallback)
{
    try {
        return std::stoi(node.child_value(name), 0, 16);
    }
    catch (const std::invalid_argument &e) {
        log::warn("could not parse %s, using fallback value %d", name, fallback);
        return fallback;
    }
}

int val_int_req(pugi::xml_node node, const char *name)
{
    try {
        return std::stoi(node.child_value(name));
    }
    catch (const std::exception &e) {
        log::err("could not parse %s, aborting...", name);
        abort();
    }
}

std::string val(pugi::xml_node node, const char *name)
{
    return node.child_value(name);
}

bool val_bool(pugi::xml_node node, const char *name)
{
    std::string v = node.child_value(name); 
    if (v == "true") {
        return true;
    } else if (v == "false") {
        return false;
    } else {
        log::warn("could not resolve '%s' as boolean, assuming false");
        return false;
    }
}

struct EncodingHint {
    std::string codec;
    bool quality_is_bitrate;
    int quality;
};

bool encoding_hint(const std::string &sample_name, EncodingHint &hint)
{
    std::regex re("\\[!rana (.+?)(?: (.+))?\\]");
    std::smatch match;

    if (std::regex_search(sample_name, match, re)) {
        hint.codec = match[1].str();
        hint.quality_is_bitrate = false;
        hint.quality = 0;

        if (match.size() >= 3) {
            auto qstr = match[2].str();

            if (qstr[0] == 'q') {
                qstr = qstr.substr(1);
            } else if (qstr.back() == 'k') {
                hint.quality_is_bitrate = true;
            } else {
                log::warn("quality specifier '%s' should be of format 'q50' or '50k'", qstr.c_str());
            }
            try {
                hint.quality = std::stoi(qstr);
            } catch (const std::exception &e) {
                log::warn("failed to parse quality hint, ignoring");
                return false;
            }
        }

        return true;
    }

    return false;
}

void find_sample_data(mz_zip_archive *zip, musfmt::Song &song)
{
    auto sample_data = std::vector<musfmt::SampleData>();

    const mz_uint filecount = mz_zip_reader_get_num_files(zip);
    mz_zip_archive_file_stat stat;
    std::regex re("^SampleData\\/Instrument(\\d+).*\\/Sample(\\d+) \\((.+)\\)\\.(.+)$");

    for (mz_uint i = 0; i < filecount; i++) {
        if (!mz_zip_reader_file_stat(zip, i, &stat)) {
            log::warn("failed to stat archive entry %d: %s",
                i, mz_zip_get_error_string(mz_zip_get_last_error(zip))
            );
            continue;
        }

        const std::string fname(stat.m_filename);
        std::smatch match;

        if (std::regex_match(fname, match, re)) {
            auto ins_i = std::stoi(match[1].str());
            auto smp_i = std::stoi(match[2].str());
            auto name = match[3].str();
            auto ext = match[4].str();

            log::info("instrument %d, sample %d: '%s.%s'", ins_i, smp_i, name.c_str(), ext.c_str());
            if (EncodingHint hint; encoding_hint(name, hint)) {
                log::info("    encoding hint: %s %d", hint.codec.c_str(), hint.quality);
            }

            musfmt::SampleData sd{};
            if (ext == "flac") {
                sd.codec = musfmt::Codec::FLAC;
            } else {
                log::warn("unrecognized sample format '%s', ignoring...", ext.c_str());
                continue;
            }

            auto bytes = stat.m_uncomp_size;
            sd.data.resize(bytes);
            if (!mz_zip_reader_extract_to_mem(zip, i, sd.data.data(), sd.data.size(), 0)) {
                log::err("failed to extract sample: %s",
                    mz_zip_get_error_string(mz_zip_get_last_error(zip))
                );
                continue;
            }

            song.ins[ins_i].smp[smp_i].sampledata_id = sample_data.size();
            sample_data.push_back(sd);
        }
    }

    song.sampledata = sample_data;
}

musfmt::Sample parse_sample(pugi::xml_node &smp)
{
    auto name = val(smp, "Name");

    musfmt::Sample sample = {};
    sample.volume = val_double(smp, "Volume",    1.0);
    sample.pan =    val_double(smp, "Panning",   0.5);
    sample.transpose = val_int(smp, "Transpose", 0);
    sample.fine =      val_int(smp, "Finetune",  0);

    auto imode = val(smp, "InterpolationMode");
    using enum musfmt::Interpolation;
    if (imode == "Linear") {
        sample.interpolation = Linear;
    } else if (imode == "None") {
        sample.interpolation = None;
    } else {
        log::warn("unsupported interpolation mode '%s', using Linear as fallback", imode.c_str());
        sample.interpolation = Linear;
    }
    if (sample.interpolation == None && val(smp, "Oversample") == "true") {
        sample.interpolation = Hybrid;
    }

    using enum musfmt::LoopMode;
    auto loopmode = val(smp, "LoopMode");
    if (loopmode == "Off") {
        sample.loop_mode = Off;
    } else if (loopmode == "Forward") {
        sample.loop_mode = Forward;
    } else if (loopmode == "Backward") {
        sample.loop_mode = Backward;
    } else if (loopmode == "PingPong") {
        sample.loop_mode = PingPong;
    } else {
        log::warn("unsupported loop mode '%s', using Off as fallback", loopmode.c_str());
        sample.loop_mode = Off;
    }

    if (val(smp, "OneShotTrigger") == "true") {
        sample.loop_mode = OneShot;
    }

    sample.loop_start = val_int(smp, "LoopStart", 0);
    sample.loop_end   = val_int(smp, "LoopEnd", -1);

    if (val(smp, "BeatSyncIsActive") == "true") {
        log::warn("beatsync is unsupported, please apply it as transpose/finetune instead");
    }

    if (val(smp, "NewNoteAction") != "Cut") {
        log::warn("new note actions (NNA) other than Cut are unsupported, "
                  "notes playing on the same column will cut each other off");
    }

    return sample;
}

double adsr_length_to_seconds(double length)
{
    return std::pow(length, 3) * 60.0;
}

void parse_instruments(pugi::xml_node &rnsong, musfmt::Song &song)
{
    size_t ins_count = 0;
    for (auto ins : rnsong.child("Instruments").children("Instrument")) {
        auto name = val(ins, "Name");

        log::info("parsing instrument %d: '%s'", ins_count, name.c_str());
        ins_count++;

        struct musfmt::Instrument instrument = {};

        for (auto smp : ins.child("SampleGenerator").child("Samples").children("Sample")) {
            musfmt::Sample sample = parse_sample(smp);

            log::info("  sample: '%s'", val(smp, "Name").c_str());
            log::info("    volume: %.2f dB", dB(sample.volume));
            log::info("    pan: %.2f", sample.pan);
            log::info("    transpose: %d", sample.transpose);
            log::info("    fine: %d", sample.fine);

            instrument.smp.push_back(sample);
        }

        bool has_volume_adsr = false;

        auto devices = ins.child("SampleGenerator")
                          .child("ModulationSets")
                          .child("ModulationSet")
                          .child("Devices");
        for (auto &n : devices.children("SampleAhdsrModulationDevice")) {
            if (val(n, "Target") != "Volume") {
                log::warn("unsupported adsr target; only volume is supported");
                continue;
            }
            if (has_volume_adsr) {
                log::warn("multiple adsr in one instrument are unsupported");
                continue;
            }
            has_volume_adsr = true;
            auto &adsr = instrument.adsr_volume;
            adsr.attack   = adsr_length_to_seconds(val_double(n.child("Attack"),  "Value", 0.0));
            adsr.hold     = adsr_length_to_seconds(val_double(n.child("Hold"),    "Value", 0.0));
            adsr.decay    = adsr_length_to_seconds(val_double(n.child("Decay"),   "Value", 0.0));
            adsr.sustain  = val_double(n.child("Sustain"), "Value", 1.0);
            adsr.release  = adsr_length_to_seconds(val_double(n.child("Release"), "Value", 0.0));

            log::info("    attack: %.3f, hold: %.3f, decay: %.3f, sustain: %.3f, release: %.3f",
                adsr.attack, adsr.hold, adsr.decay, adsr.sustain, adsr.release);
        }

        song.ins.push_back(instrument);
    }
}

void parse_effects(pugi::xml_node &devices, std::vector<musfmt::Effect> &effects)
{
    for (auto &n : devices.children("AudioPluginDevice")) {
        struct musfmt::Effect fx;
        auto pid = val(n, "PluginIdentifier");
        using enum musfmt::EffectType;
        if (pid == "ranaReverb") {
            fx.type = Reverb;
        } else if (pid == "ranaDelay") {
            fx.type = Delay;
        } else if (pid == "ranaLowpass") {
            fx.type = Lowpass;
        } else if (pid == "ranaHighpass") {
            fx.type = Highpass;
        } else if (pid == "ranaDistortion") {
            fx.type = Distortion;
        } else if (pid == "ranaBitcrush") {
            fx.type = Bitcrush;
        } else if (pid == "ranaCompressor") {
            fx.type = Compressor;
        } else if (pid == "ranaGalactic") {
            fx.type = Galactic;
        } else {
            log::warn("ignoring unsupported plugin %s", pid.c_str());
            continue;
        }

        for (auto &p : n.child("Parameters").children("Parameter")) {
            fx.param.push_back(val_double(p, "Value", 0.0));
        }

        effects.push_back(fx);
    }
}

musfmt::MixerTrack parse_track(pugi::xml_node &t)
{
    musfmt::MixerTrack track{};
    track.name = val(t, "Name");
    track.columns = val_int(t, "NumberOfVisibleNoteColumns", 1);

    auto soloed = val_bool(t, "Soloed");

    auto devices = t.child("FilterDevices").child("Devices");
    auto mixer = devices.child("TrackMixerDevice");
    auto is_active =  val_double(mixer.child("IsActive"),    "Value", 1.0) >= 1;
    auto pre_pan    = val_double(mixer.child("Panning"),     "Value", 0.5);
    auto pre_volume = val_double(mixer.child("Volume"),      "Value", 1.0);
    track.pan    =    val_double(mixer.child("PostPanning"), "Value", 0.5);
    track.volume =    val_double(mixer.child("PostVolume"),  "Value", 1.0);
    auto surround =   val_double(mixer.child("Surround"),    "Value", 0.0);

    if (soloed) {
        log::warn("track is soloed");
    }
    if (!is_active) {
        log::warn("track is not active");
    }
    if (pre_pan != 0.5) {
        log::warn("pre pan is not supported");
    }
    if (pre_volume != 1.0) {
        log::warn("pre volume is not supported");
    }
    if (surround > 0.0) {
        log::warn("mixer track width parameter is unsupported");
    }

    parse_effects(devices, track.fx);

    return track;
}

musfmt::Mixer parse_mixer(pugi::xml_node &rnsong)
{
    musfmt::Mixer mixer{};

    auto tracks = rnsong.child("Tracks").children("SequencerTrack");
    for (auto &track : tracks) {
        auto t = parse_track(track);
        mixer.tracks.push_back(t);

        log::info("track %d: '%s'", mixer.tracks.size(), t.name.c_str());
        log::info("  volume: %.2f dB", dB(t.volume));
        log::info("  pan: %f",       t.pan);
    }

    auto master = rnsong.child("Tracks").child("SequencerMasterTrack");
    auto devices = master.child("FilterDevices").child("Devices");
    auto master_device = devices.child("MasterTrackMixerDevice");
    mixer.master_volume = val_double(master_device.child("PostVolume"), "Value", 1.0);
    if (val_double(master_device.child("PostPanning"), "Value", 0.5) != 0.5) {
        log::warn("panning on master? be serious");
    }

    parse_effects(devices, mixer.master_fx);

    return mixer;
}

int parse_note(pugi::xml_node note_column)
{
    std::string notestr = note_column.child_value("Note");
    if (notestr.size() != 3) {
        return -1;
    }
    if (notestr == "OFF") return 0;

    char octstr[2] = {notestr[2], 0};
    auto octave = std::stoi(octstr);
    int note = 0;
    switch (notestr[0]) {
        case 'C': note = 0; break;
        case 'D': note = 2; break;
        case 'E': note = 4; break;
        case 'F': note = 5; break;
        case 'G': note = 7; break;
        case 'A': note = 9; break;
        case 'B': note = 11; break;
        default:
            log::err("could not parse note '%s'", notestr.c_str());
            abort();
    }
    
    if (notestr[1] == '#') {
        note++;
    }

    return note + 1 + octave * 12;
}

void parse_patterns(pugi::xml_node &rnsong, musfmt::Song &song)
{
    auto patterns = rnsong.child("PatternPool").child("Patterns").children("Pattern");
    for (auto &pattern : patterns) {
        musfmt::Pattern p{};
        p.lines = val_int_req(pattern, "NumberOfLines");
        int track_i = 0;

        for (auto &track : pattern.child("Tracks").children("PatternTrack")) {
            std::vector<musfmt::PatternChannel> ch(song.mixer.tracks[track_i].columns);
            int prev_index[12] = {0};
            for (auto &line : track.child("Lines").children("Line")) {
                using enum rana::musfmt::CommandType;

                auto line_index = line.attribute("index").as_int(0);
                int col_i = 0;
                for (auto &nc : line.child("NoteColumns").children("NoteColumn")) {
                    if (ch.size() <= col_i) {
                        log::warn("discarding notes from track %d after column %d", track_i, ch.size());
                        break;
                    }

                    rana::musfmt::Command cmd{};

                    if (auto delta = line_index - prev_index[col_i]; delta != 0) {
                        if (delta < 0) {
                            log::err("lines are not sequential, aborting...");
                            abort();
                        }

                        cmd.type = SleepLines;
                        cmd.param_xy = delta;
                        ch[col_i].rows.push_back(cmd);
                        prev_index[col_i] = line_index;
                    }

                    if (auto note = parse_note(nc); note >= 0) {
                        cmd.type = Note;
                        cmd.note = note;
                        ch[col_i].rows.push_back(cmd);
                    }

                    if (!nc.child("Instrument").empty()) {
                        cmd.type = Instrument;
                        cmd.param_xy = val_hex(nc, "Instrument", 0);
                        ch[col_i].rows.push_back(cmd);
                    }

                    if (!nc.child("Volume").empty()) {
                        cmd.type = Volume;
                        cmd.param_xy = val_hex(nc, "Volume", 128);
                        ch[col_i].rows.push_back(cmd);
                    }

                    if (val(nc, "EffectNumber") == "0A") {
                        cmd.type = FxArp;
                        cmd.param_xy = val_hex(nc, "EffectValue", 0);
                        ch[col_i].rows.push_back(cmd);
                    }

                    if (val(nc, "EffectNumber") == "0V") {
                        cmd.type = FxVibrato;
                        cmd.param_xy = val_hex(nc, "EffectValue", 0);
                        ch[col_i].rows.push_back(cmd);
                    }

                    col_i++;
                }

                for (auto &ec : line.child("EffectColumns").children("EffectColumn")) {
                    rana::musfmt::Command cmd{};

                    if (val(ec, "Number") == "0A") {
                        cmd.type = FxArp;
                        cmd.param_xy = val_hex(ec, "Value", 0);
                    } else if (val(ec, "Number") == "0V") {
                        cmd.type = FxVibrato;
                        cmd.param_xy = val_hex(ec, "Value", 0);
                    } else if (val(ec, "Number") == "ZT") {
                        if (auto delta = line_index - prev_index[0]; delta != 0) {
                            if (delta < 0) {
                                log::err("lines are not sequential, aborting...");
                                abort();
                            }

                            cmd.type = SleepLines;
                            cmd.param_xy = delta;
                            ch[0].rows.push_back(cmd);
                            prev_index[0] = line_index;
                        }
                        cmd.type = FxTempo;
                        cmd.param_xy = val_hex(ec, "Value", 0);
                        ch[0].rows.push_back(cmd);
                        prev_index[0] = line_index;
                        continue;
                    } else {
                        continue;
                    }

                    for (int i = 0; i < ch.size(); i++) {
                        if (auto delta = line_index - prev_index[i]; delta != 0) {
                            if (delta < 0) {
                                log::err("lines are not sequential, aborting...");
                                abort();
                            }

                            cmd.type = SleepLines;
                            cmd.param_xy = delta;
                            ch[i].rows.push_back(cmd);
                            prev_index[i] = line_index;
                        }
                        ch[i].rows.push_back(cmd);
                        prev_index[col_i] = line_index;
                    }
                }

            }
            track_i++;
            p.ch.insert(p.ch.end(), ch.begin(), ch.end());
        }
        song.patterns.push_back(p);
    }
}

void parse_sequence(pugi::xml_node &rnsong, musfmt::Song &song)
{
    auto seq = rnsong.child("PatternSequence");
    for (auto &n : seq.child("SequenceEntries").children("SequenceEntry")) {
        auto pattern = val_int(n, "Pattern", 0);
        song.sequence.push_back(pattern);
    }

    auto loop = seq.child("LoopSelection");
    int start = val_int(loop, "CursorPos", -1);
    int end   = val_int(loop, "RangePos",  -1);
    if (start < 0 || end < 0) { // no loop region
        song.loop_start = 0;
        song.loop_end = song.patterns.size() - 1;
    } else {
        song.loop_start = start;
        song.loop_end = end;
    }
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

    log::info("working on file '%s'", infile);

    size_t xml_len;

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, infile, 0)) {
        log::err("failed to open archive: %s",
            mz_zip_get_error_string(mz_zip_get_last_error(&zip))
        );
        return -1;
    };

    char *xml = (char *)mz_zip_reader_extract_file_to_heap(&zip, "Song.xml", &xml_len, 0);

    if (xml == nullptr) {
        log::err("failed to read Song.xml from the archive: %s",
            mz_zip_get_error_string(mz_zip_get_last_error(&zip))
        );
        return -1;
    }

    pugi::xml_document doc;
    auto result = doc.load_buffer(xml, xml_len);
    if (result) {
        log::info("parsing xml OK");
    } else {
        log::err("failed to parse xml: %s", result.description());
        return -1;
    }

    auto rnsong = doc.child("RenoiseSong");
    auto sd = rnsong.child("GlobalSongData");

    auto song = musfmt::Song();
    song.bpm = val_double(sd, "BeatsPerMin", 140);
    song.beat_lines = val_int(sd, "LinesPerBeat", 4);
    song.line_ticks = val_int(sd, "TicksPerLine", 12);

    parse_instruments(rnsong, song);

    find_sample_data(&zip, song);

    song.mixer = parse_mixer(rnsong);
    parse_patterns(rnsong, song);
    parse_sequence(rnsong, song);

    auto ser = Serializer();
    song.serialize(ser);

    log::info("writing output to '%s'...", outfile);
    auto serialized = ser.data();
    auto bytes = rana::writefile(serialized, outfile);
    log::info("wrote %d bytes.", bytes);

    std::free(xml);
    return 0;
}

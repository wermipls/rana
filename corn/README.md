# corn

`corn` is a tool that converts Renoise songs (.xrns) to a format suitable for `rana` music engine playback

Basic usage:
```sh
./corn input.xrns output.ranamus
```

## Feature support/limitations

### Global
- Global Groove is NOT supported, use `ZTxx` instead for swing tempo

### Instruments
- Modulation: Volume AHDSR (note off always cuts note, so release is not handled)
- Limit of one sample per instrument
- **NO** keyzones (most notably, Base Note is always treated like `C-4`, so pitch will sound wrong after conversion if it's different)

Nothing else is supported; if using any more complex DSP, bounce your instruments to basic sample data and/or substitute modulation with pattern commands.

### Samples
- Sample data encoded in FLAC
- Transpose
- Finetune
- Volume
- Loop: Off, Forward
- NNA: Cut 
- Interpolation: Linear, None

#### Sample encoding hints
The converter parses encoding hints embedded in sample names. This allows easy fine-tuning of final sample bitrate while keeping the original, high quality samples in the project, as well as using codecs not supported natively by Renoise. Hint format is `[!rana codec quality]`, for example `[!rana opus 32k]` would encode the sample as Opus with target bitrate of 32kbps.

As of current, no encoding hints are acted upon by the converter.

### Patterns

Supported columns:
- Volume<sup>1</sup>
- Instrument
- Note effect
- Track effect

<sup>1</sup>only numeric values are supported, no effects

Supported effects:
- `Vxy` - Vibrato
- `Gxx` - Glide
- `Uxx`, `Dxx` - Pitch slide up/down
- `ZTxx` - Tempo
- `Axy` - Arpeggio

Limitations:
- Effects can't be placed on master/send columns

### Mixer

A range of in-engine effects are supported, available in VST form for Renoise playback (`vst` directory):
- ranaBitcrush - bitdepth/rate reduction
- ranaCompressor - basic compressor that doubles as a limiter, can be quite gentle or very aggressive going into softclip territory
- ranaDelay - very simple delay
    - given a delay value in milliseconds, delay parameter value in percent (ready to be pasted in Renoise) would be `milliseconds / 50`
    - tempo sync is not supported; for 4/4 time signature, calculate delay parameter value as `60 / bpm * 4 * delay`, where delay is your fractional delay. For example, a tempo of 140 and delay of 3/16, delay in seconds would be `60 / 140 * 4 / 16 * 3` or ~321 ms.
- ranaDistortion - distortion unit with 4 modes, in order:
    - Hardclip - signal gets clamped to (-1.0, 1.0),
    - Shape -  signal gets nonlinearly bent,
    - Fold - signal "folds" the other way when going out of range
    - BadFold - buggy supposed-to-be implementation of above
- ranaGalactic - port of [airwindows' Galactic](https://www.airwindows.com/galactic/), an absolutely massive reverb
- ranaHighpass, ranaLowpass - single pole HP/LP filters
- ranaReverb - reverb based on Freeverb

Limitations:
- Tracks only support post-FX volume/pan
- No track delay
- Track groups are not supported and need to be removed for proper conversion
- Sends are not supported
- No other effects are supported
- No parameter automation from song level
#pragma once

#include <vector>
#include <string>
#include "format.hpp"

namespace rana {
namespace audio {

struct RenderFx {
    char str[5] = "    ";
};

struct RenderRow {
    char note[4] = "   ";
    char ins = ' ';
    char vol[3] = "  ";
    std::vector<RenderFx> fx;

    inline std::string render() {
        std::string s;
        s += note;
        s += ' ';
        s += ins ? ins : ' ';
        s += ' ';
        s += vol;

        for (auto &f : fx) {
            s += f.str;
        }

        return s;
    }
};

struct RenderColumn {
    std::vector<RenderRow> row;

    int getMaxCharWidth() {
        int mw = 3;
        for (auto &r : row) {
            if (r.ins != ' ') {
                mw = std::max(mw, 4);
            }
            if (r.vol[0] != ' ') {
                mw = std::max(mw, 6);
            }

            if (r.fx.size() > 0) {
                mw = std::max(mw, int(6 + r.fx.size() * 4));
            }
        }

        return mw;
    }
};

struct RenderPtn {
    std::vector<RenderColumn> col;
};

static inline char num2chr(int a)
{
    if (a <= 9) {
        return '0' + a;
    } else {
        return 'A' + a - 10;
    }
}

static inline void render_tohex(uint8_t val, char *str)
{
    auto l = val & 0xf;
    auto h = val >> 4;
    str[0] = num2chr(h);
    str[1] = num2chr(l);
}

static inline void append_fx(const musfmt::Command &cmd, RenderRow &row)
{
    using namespace musfmt;
    RenderFx fx;
    fx.str[0] = ' ';
    auto &b = fx.str[0];
    auto &c = fx.str[1];
    render_tohex(cmd.param_xy, &fx.str[2]);
    switch (cmd.type) {
        case CommandType::FxArp:                c = 'A'; break;
        case CommandType::FxVibrato:            c = 'V'; break;
        case CommandType::FxFadeout:            c = 'O'; break;
        case CommandType::FxFadein:             c = 'I'; break;
        case CommandType::FxReverse:            c = 'R'; break;
        case CommandType::FxOffset:             c = 'S'; break;
        case CommandType::FxTempo:              c = 'T'; b = 'Z'; break;
        case CommandType::FxGlide:              c = 'G'; break;
        case CommandType::FxSlideUp:            c = 'U'; break;
        case CommandType::FxSlideDown:          c = 'D'; break;
        case CommandType::FxMixerEffectParam:
            b = num2chr(cmd.param.x);
            c = num2chr(cmd.param.y);
            render_tohex(cmd.mixerfx_value, &fx.str[2]);
            break;
    }
    row.fx.push_back(fx);
} 

static inline RenderPtn render_pattern(const musfmt::Pattern p)
{
    using namespace musfmt;
    RenderPtn ptn{};

    for (auto &ch : p.ch) {
        ptn.col.resize(ptn.col.size() + 1);
        auto &col = ptn.col[ptn.col.size() - 1];

        auto &rows = col.row;
        rows.resize(1);
        int i = 0;

        for (auto &cmd : ch.rows) {
            auto &row = rows[i];
            if (cmd.type == CommandType::SleepLines) {
                rows.resize(rows.size() + cmd.param_xy);
                i += cmd.param_xy;
                continue;
            }

            if (cmd.type == CommandType::Note || cmd.type == CommandType::NoteLegato) {
                auto note = (cmd.note - 1) % 12;
                auto oct  = (cmd.note - 1) / 12;
                row.note[1] = '-'; row.note[2] = '0';
                switch (note) {
                    case 0:  row.note[0] = 'C'; break;
                    case 1:  row.note[0] = 'C'; row.note[1] = '#'; break;
                    case 2:  row.note[0] = 'D'; break;
                    case 3:  row.note[0] = 'D'; row.note[1] = '#'; break;
                    case 4:  row.note[0] = 'E'; break;
                    case 5:  row.note[0] = 'F'; break;
                    case 6:  row.note[0] = 'F'; row.note[1] = '#'; break;
                    case 7:  row.note[0] = 'G'; break;
                    case 8:  row.note[0] = 'G'; row.note[1] = '#'; break;
                    case 9:  row.note[0] = 'A'; break;
                    case 10: row.note[0] = 'A'; row.note[1] = '#'; break;
                    case 11: row.note[0] = 'B'; break;
                }
                row.note[2] += oct;

                if (cmd.note == 0) {
                    row.note[0] = 'O'; row.note[1] = 'f'; row.note[2] = 'f';
                }
            }

            switch (cmd.type) {
                case CommandType::Instrument:
                    row.ins = num2chr(cmd.param_xy);
                    break;
                case CommandType::Volume:
                    render_tohex(cmd.param_xy, row.vol); break;
                    break;
                case CommandType::Pan:
                    break; // FIXME unhandled
                case CommandType::FxArp:
                case CommandType::FxVibrato:
                case CommandType::FxFadeout:
                case CommandType::FxFadein:
                case CommandType::FxReverse:
                case CommandType::FxOffset:
                case CommandType::FxTempo:
                case CommandType::FxGlide:
                case CommandType::FxSlideUp:
                case CommandType::FxSlideDown:
                case CommandType::FxMixerEffectParam:
                    append_fx(cmd, row);
                    break;
            }
        }
    }

    return ptn;
}

}
}

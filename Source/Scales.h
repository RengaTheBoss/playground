#pragma once

#include <cmath>
#include <cstdlib>

namespace pg
{
struct ScaleDef
{
    const char* name;
    int count;
    int iv[7];
};

inline constexpr int numScales = 10;

inline const ScaleDef* scaleTable()
{
    static const ScaleDef table[numScales] = {
        { "Major",          7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",          7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",         7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",       7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",         7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",     7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Harmonic Minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Pentatonic Maj", 5, { 0, 2, 4, 7, 9 } },
        { "Pentatonic Min", 5, { 0, 3, 5, 7, 10 } },
        { "Blues",          6, { 0, 3, 5, 6, 7, 10 } },
    };
    return table;
}

inline int floorDiv (int a, int b)
{
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

/** Semitone offset from the root for a scale degree (0 = root, may be negative or exceed one octave). */
inline int degreeToSemitone (const ScaleDef& s, int degree)
{
    const int oct = floorDiv (degree, s.count);
    const int idx = degree - oct * s.count;
    return oct * 12 + s.iv[idx];
}

/** Moves a MIDI note to the nearest note of the scale (ties go downwards). */
inline int snapToScale (const ScaleDef& s, int root, int note)
{
    const int rel = note - root;
    const int oct = floorDiv (rel, 12);
    const int pc = rel - oct * 12;

    int best = pc;
    int bestDist = 99;

    for (int k = 0; k < s.count; ++k)
    {
        for (int off = -12; off <= 12; off += 12)
        {
            const int cand = s.iv[k] + off;
            const int d = std::abs (cand - pc);

            if (d < bestDist || (d == bestDist && cand < best))
            {
                best = cand;
                bestDist = d;
            }
        }
    }

    const int result = root + oct * 12 + best;
    return result < 0 ? 0 : (result > 127 ? 127 : result);
}

inline const char* const* noteNames()
{
    static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}
} // namespace pg

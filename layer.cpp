#include "layer.h"
#include "looper.h"
#include "midi.h"

namespace midiate
{

namespace
{

#define SEQUENCE(...) { __VA_ARGS__ , -1 } // '-1' indicates the end of the sequence

const char __styles[][Layer::Pitches + 1] = // '+1' for the end indicator
    {
        SEQUENCE(1, 3, 5),          // up
        SEQUENCE(5, 3, 1),          // down
        SEQUENCE(1, 3, 5, 3),       // up down
        SEQUENCE(5, 3, 1, 3),       // down up
        SEQUENCE(1, 3, 5, 3, 1),    // up & down
        SEQUENCE(5, 3, 1, 3, 5),    // down & up
        SEQUENCE(1, 5, 3),          // converge
        SEQUENCE(3, 1, 5),          // diverge
        SEQUENCE(1, 5, 3, 5),       // pinky up
        SEQUENCE(1, 3, 1, 5),       // thumb up
    };

#define ASSERT(x, expected) static_assert(static_cast<int>(x) == (expected), "Expected midiate::" #x " to be equal to " #expected);

ASSERT(Style::Up,           0);
ASSERT(Style::Down,         1);
ASSERT(Style::UpDown,       2);
ASSERT(Style::DownUp,       3);
ASSERT(Style::UpAndDown,    4);
ASSERT(Style::DownAndUp,    5);
ASSERT(Style::Converge,     6);
ASSERT(Style::Diverge,      7);
ASSERT(Style::PinkyUp,      8);
ASSERT(Style::ThumbUp,      9);

static_assert(sizeof(__styles) / sizeof(__styles[0]) == 10, "Expected 10 styles to be declared");

#define THIRD(i)    (i / 3.f)
#define QUARTER(i)  (i / 4.f)

const float __rhythms[][Layer::Subdivisions + 1] = // '+1' for the end indicator
    {
        SEQUENCE(QUARTER(0), QUARTER(1), QUARTER(2), QUARTER(3)),   // 1/16 1/16 1/16 1/16
        SEQUENCE(QUARTER(0), QUARTER(1), QUARTER(2)),               // 1/16 1/16 1/8
        SEQUENCE(QUARTER(0),             QUARTER(2), QUARTER(3)),   // 1/8       1/16 1/16
        SEQUENCE(QUARTER(0), QUARTER(1),             QUARTER(3)),   // 1/16 1/8       1/16
        SEQUENCE(            QUARTER(1), QUARTER(2), QUARTER(3)),   //      1/16 1/16 1/16
        SEQUENCE(THIRD(0),      THIRD(1),      THIRD(2)),           // 1/8th note triplet
        SEQUENCE(THIRD(0),                     THIRD(2)),           // 1/8th note swung triplet
    };

ASSERT(Rhythm::A,   0);
ASSERT(Rhythm::B,   1);
ASSERT(Rhythm::C,   2);
ASSERT(Rhythm::D,   3);
ASSERT(Rhythm::E,   4);
ASSERT(Rhythm::F,   5);
ASSERT(Rhythm::G,   6);

static_assert(sizeof(__rhythms) / sizeof(__rhythms[0]) == 7, "Expected 7 rhythms to be declared");

} //

Layer::Layer(const Triad & triad, Style style, Rhythm rhythm, char start, char tag) :
    tag(tag)
{
    int i;
    
    char const * const degrees = __styles[static_cast<int>(style)];

    for (i = 0; degrees[i] != -1; ++i)
    {
        _pitches[i] = triad.degree(degrees[i]);
    }

    for (; i < Layer::Pitches; ++i)
    {
        _pitches[i] = -1; // mark as unused
    }

    float const * const portions = __rhythms[static_cast<int>(rhythm)];

    for (i = 0; portions[i] != -1; ++i)
    {
        _subdivisions[i] = (start + static_cast<int>(portions[i] * Looper::Subdivisions)) % Looper::Subdivisions;
    }

    for (; i < Layer::Subdivisions; ++i)
    {
        _subdivisions[i] = -1; // mark as unused
    }
}

void Layer::play(char now)
{
    for (auto subdivision : _subdivisions)
    {
        if (subdivision == -1)
        {
            break; // iterated through all subdivisions
        }

        if (subdivision == now)
        {
            midi::play(_pitches[_pitch]);

            _pitch = (_pitch + 1) % Layer::Pitches;
            
            if (static_cast<int>(_pitches[_pitch]) == -1)
            {
                _pitch = 0;
            }

            break; // our job here is done
        }
    }
}

} // midiate
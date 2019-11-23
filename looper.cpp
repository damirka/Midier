#include "looper.h"
#include "midi.h"
#include "scale.h"
#include "triad.h"

#include <Arduino.h>

namespace midiate
{

Looper::Looper(const Config & config) :
    state(State::Wander),
    _config(config),
    _layers(),
    _beat(),
    _bars(0)
{}

char Looper::start(char degree)
{
    for (char i = 0; i < sizeof(_layers) / sizeof(Layer); ++i)
    {
        auto & layer = _layers[i];

        if (layer.tag != -1)
        {
            continue; // this layer is used
        }

        layer = Layer(
            degree,
            _config.style,
            _config.rhythm,
            _beat,
            i // tag
        );

        if (state == State::Record || state == State::Overlay)
        {
            layer.state = Layer::State::Record;
        }

        return i;
    }

    return -1; // there's no place for a new layer
}

void Looper::stop(char tag)
{
    for (auto & layer : _layers)
    {
        if (layer.tag != tag)
        {
            continue;
        }

        if (layer.state == Layer::State::Wander)
        {
            layer.tag = -1; // mark as unused
        }
        else if (layer.state == Layer::State::Record)
        {
            layer.state = Layer::State::Playback;
        }

        // do nothing if the layer is already in playback mode (let it keep playbacking)
        // this could happen when the button is kept being pressed even after the entire
        // recorded loop (32 bars) is over
    }
}

void Looper::undo()
{
    if (state == State::Wander)
    {
        return; // nothing to do when wandering
    }

    // we want to stop the last recorded (or being recorded) layer
    // we cannot tell for sure which layer was the last one to be recorded,
    // so we assume it is the layer with the highest tag (and the highest index)

    for (char i = sizeof(_layers) / sizeof(Layer); i > 0; --i)
    {
        auto & layer = _layers[i - 1];

        if (layer.tag == -1)
        {
            continue;
        }

        if (layer.state == Layer::State::Record || layer.state == Layer::State::Playback)
        {
            layer.tag = -1;
            break;
        }
    }

    // now check if there is no more recorded layers

    for (const auto & layer : _layers)
    {
        if (layer.tag == -1)
        {
            continue;
        }

        if (layer.state == Layer::State::Record || layer.state == Layer::State::Playback)
        {
            return;
        }
    }

    // seems like there are no such layers, go back to wander mode
    state = State::Wander;
}

void Looper::run(callback_t callback)
{
    auto previous = state;

    while (true)
    {
        // we disable interrupts because we don't want any user actions to interfere the main logic
        // both 'state' and the '_layers' may be modified, and '_beat' may be accessed via interrupts
        noInterrupts();

        if (state == State::Record && previous == State::Wander) // just starting to record
        {
            _beat.bar = 0; // start counting bars
            _beat.start = _beat.subdivision;

            for (auto & layer : _layers) // start recording all layers
            {
                if (layer.tag == -1)
                {
                    continue; // this layer is not used
                }

                layer.state = Layer::State::Record;
            }
        }
        else if (state == State::Wander && previous != State::Wander)
        {
            _beat.bar = _beat.start = -1; // stop counting bars
            _bars = 0; // reset the # of recorded bars

            for (auto & layer : _layers) // revoke all layers
            {
                layer.tag = -1;
            }
            
            callback(-1); // clear the bar
        }

        if (_beat.subdivision == _beat.start)
        {
            if (state == State::Record && _bars < sizeof(Moment::bars) * 8) // maximum # of bars
            {
                ++_bars; // increase the # of recorded bars when (recording and) entering a new bar
            }

            if (state == State::Record || state == State::Playback || state == State::Overlay)
            {
                if (_beat.bar == _bars) // just passed the # of recorded bars
                {
                    _beat.bar = 0;
                }
            
                callback(_beat.bar);
            }
        }

        for (auto & layer : _layers)
        {
            if (layer.tag == -1)
            {
                continue; // unused layer
            }

            Pitch pitch;
            if (!layer.play(_beat, /* out */ pitch))
            {
                continue;
            }

            const auto quality = scale::quality(_config.mode, pitch.chord());

            char number = 24; // C0
            number += 12 * (_config.octave - 1); // go to the right octave
            number += static_cast<char>(_config.note) + static_cast<char>(_config.accidental); // go to the root of the scale
            number += static_cast<char>(scale::interval(_config.mode, pitch.chord())); // go to the root of the chord
            number += static_cast<char>(triad::interval(quality, pitch.note())); // go to the note in the chord

            midi::play(number);
        }

        ++_beat;

        for (auto & layer : _layers)
        {
            if (layer.tag == -1)
            {
                continue; // unused layer
            }

            if (_beat == layer.start) // the layer has completed a full loop
            {
                if (layer.state == Layer::State::Record)
                {
                    layer.state = Layer::State::Playback; // stop recording the layer and start playing it back
                }

                if (layer.state == Layer::State::Playback) // not 'else if'
                {
                    // we reset the layer's pitches and moments because we want the layer to playback exactly what recorded
                    layer.reset();
                }
            }
        }

        previous = state;
        
        // enable interrupts as we are done with the main logic and no need for locks anymore
        interrupts();

        delay(60.f / static_cast<float>(_config.bpm) * 1000.f / static_cast<float>(Beat::Subdivisions));
    }
}

} // midiate

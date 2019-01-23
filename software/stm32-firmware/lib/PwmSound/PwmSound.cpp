/******************************************************************************
 * File:      PwmSound.cpp
 * Author:    Paul Griffith
 * Created:   25 Mar 2014
 * Last Edit: see below
 * Version:   see below
 *
 * Description:
 * Class to play tones, various sounds, musical notes and simple tunes using
 * a PWM channel. Inspired by Jim Hamblem's Speaker class. Thanks Jim!
 *
 * Refer to the tutorial "Using a Speaker for Audio Output" in the Cookbook.
 * The mbed LPC1768 PWM pins will drive a small speaker without amplification.
 * Connect speaker via a 220R resistor and 100uF 10V capacitor (+ve to mbed).
 *
 * Copyright (c) 2014 Paul Griffith, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Modifications:
 * Ver  Date    By  Details
 * 0.00 25Mar14 PG  File created.
 * 1.00 30Mar14 PG  Initial release.
 * 2.00 06May14 PG  Added play() etc to support MML music. Removed tune() etc.
 *
 ******************************************************************************/

#include "mbed.h"
#include "PwmSound.h"
// #include "FastOut.h"

// extern Serial pc;       //for debugging, comment out if not needed
// FastOut<LED1> led1;

// Constructor

PwmSound::PwmSound(PinName pin) : _pin(pin) {
	_dutyCycle = 0.5;
	_pin = 0.0;
	_playing = false;
}

// Play a tone on output pin
//
// Parameters:
//    frequency - frequency of tone in Hz
//    duration - duration of tone in seconds
//               if duration = 0.0, tone continues in background until stopped
// Returns: nothing
// Uses: current duty cycle

void PwmSound::tone(float frequency, float duration) {
    _pin.period(1.0 / frequency);
    _pin = _dutyCycle;
    if (duration == 0.0) {
        _playing = true;
        return;
    }
    wait(duration);
    _pin = 0.0;
}

// Stop background tone or sound generation

void PwmSound::stop(void) {
    _playing = false;
    _pin = 0.0;
}

// Set timbre (tonal quality)
//
// Parameters:
//    timbre - (1-4): sets PWM duty cycle to 12.5%, 25%, 37.5% or 50%
// Returns: nothing

void PwmSound::timbre(int t) {
	if (t >= 1 && t <= 4) {
		_dutyCycle = t / 8.0;
	}
}
// Beeps of various types and other sounds
// Note: All sounds below except phone permit continuous sound in background
//       To invoke this call the function with a zero parameter
//       Call stop() to end the sound
//
// Parameters:
//    n - number of cycles, 0 for continuous sound in background (not phone)
// Returns: nothing

void PwmSound::bip(int n) {
    if (n == 0) {
        _setup(1047.0, 0.1, 0.0, 0.03);
        return;
    }
    for (int i = 0; i < n; i++) {
        tone(1047.0, 0.10);
        wait(0.03);
    }
}

void PwmSound::bop(int n) {
    if (n == 0) {
        _setup(700.0, 0.1, 0.0, 0.03);
        return;
    }
    for (int i = 0; i < n; i++) {
        tone(700.0, 0.10);
        wait(0.03);
    }
}

void PwmSound::beep(int n) {
    if (n == 0) {
        _setup(969.0, 0.3, 0.0, 0.1);
        return;
    }
    for (int i = 0; i < n; i++) {
        tone(969.0, 0.3);
        wait(0.1);
    }
}

void PwmSound::bleep(int n) {
    if (n == 0) {
        _setup(800.0, 0.4, 0.0, 0.1);
        return;
    }
    for (int i = 0; i < n; i++) {
        tone(800.0, 0.4);
        wait(0.1);
    }
}

void PwmSound::buzz(int n) {
    if (n == 0) {
        _setup(1900.0, 0.01, 300.0, 0.01);
        return;
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < 20; j++) {
            tone(1900.0, 0.01);
            tone(300.0, 0.01);
        }
    }
}

void PwmSound::siren(int n) {
    if (n == 0) {
        _setup(969.0, 0.5, 800.0, 0.5);
        return;
    }
    for (int i = 0; i < n; i++) {
        tone(969.0, 0.5);
        tone(800.0, 0.5);
    }
}

void PwmSound::trill(int n) {
    if (n == 0) {
        _setup(969.0, 0.05, 800.0, 0.05);
        return;
    }
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            tone(800.0, 0.05); //make the trills sound continouus
        }
        tone(969.0, 0.05);
        tone(800.0, 0.05);
        tone(969.0, 0.05);
        tone(800.0, 0.05);
        tone(969.0, 0.05);
        tone(800.0, 0.05);
        tone(969.0, 0.05);
        tone(800.0, 0.05);
        tone(969.0, 0.05);
    }
}

void PwmSound::phone(int n) {
    for (int i = 0; i < n; i++) {
        trill();
        wait(0.10);
        trill();
        wait(0.7);
    }
}   

// Continuous sound setup and callback routines
// _sustain() has been optimised for speed. On a 96MHz LPC1768 it takes 8.5us.
// Non-optimised version with floating point freqency & duration took 11.4us.
// Execution times measured with 'scope on LED1 pin.

void PwmSound::_setup(float freq1, float dur1, float freq2, float dur2) {
    _period1 = (int) (1000000.0 / freq1);
    _period2 = (int) (1000000.0 / freq2);
    _dur1 = (unsigned int) (1000000.0 * dur1);
    _dur2 = (unsigned int) (1000000.0 * dur2);
    _phase = false;
    _sustainTmo.attach_us(callback(this, &PwmSound::_sustain), _dur1);
    _pin.period_us(_period1);      //start the sound
    _pin = _dutyCycle;
    _playing = true;
}
        
void PwmSound::_sustain(void) {
    //led1 = 1;
    if (_playing == false) {
        //kill pwm and no more callbacks
        _pin = 0.0;
    } else {
        _phase = !_phase;
        if (_phase) {
            _pin.period_us(_period2);
            _pin = _dutyCycle;
            _sustainTmo.attach_us(callback(this, &PwmSound::_sustain), _dur2);
        } else {
            _pin.period_us(_period1);
            _pin = _dutyCycle;
            _sustainTmo.attach_us(callback(this, &PwmSound::_sustain), _dur1);
        }
    }
    //led1 = 0;
}

// END of PwmSound.cpp

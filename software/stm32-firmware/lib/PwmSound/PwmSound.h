/******************************************************************************
 * File:      PwmSound.h
 * Author:    Paul Griffith
 * Created:   25 Mar 2014
 * Last Edit: see below
 * Version:   see below
 *
 * Description:
 * Definitions for PwmSound class.
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
 * 1.00 30Mar14 PG  Initial release
 * 2.00 06May14 PG  Added play() etc to support MML music. Removed tune() etc.
 *
 ******************************************************************************/

#ifndef MBED_PWMSOUND_H
#define MBED_PWMSOUND_H

#include "mbed.h"

class PwmSound {
//private:

public:
    PwmSound(PinName pin);

    void tone(float frequency, float duration = 0.0);     //tones
    void stop(void);
    void timbre(int timbre);

    void bip(int n = 1);    //beeps and other sounds
    void bop(int n = 1);
    void beep(int n = 1);
    void bleep(int n = 1);
    void buzz(int n = 1);
    void siren(int n = 1);
    void trill(int n = 1);
    void phone(int n = 1);

    int play(const char* m, int options = 0);		//play tune in MML format

private:
    PwmOut _pin;
    float _dutyCycle;   //PWM duty cycle 0-50%

    //the following support continuous two-tone sounds in background
    void _setup(float freq1, float dur1, float freq2, float dur2);
    void _sustain(void);    
    Timeout _sustainTmo;
    int _period1;           //in us
    unsigned int _dur1;     //in us
    int _period2;
    unsigned int _dur2;
    bool _phase;
    bool _playing;

    //the following support play
    void _note(int number, int length, int dots = 0);
    char _getChar(void);
    char _nextChar(void);
    int _getNumber(void);
    int _getDots(void);
    int _octave;	//current octave
    int _shift;		//octave shift from < and >
    int _tempo;     //pace of music in beats per minute (32-255)
                    //one beat equals one quarter note (ie a crotchet)
    int _length;    //length of note (1-64), 1 = whole note, 4 = quarter etc
    float _1dot;	//note length extension factors
    float _2dots;
    float _3dots;
    int _style;     //music style (1-8), 6 = Staccato, 7 = Normal, 8 = Legato
    const char* _mp;		//current position in music string
    char _nextCh;
    bool _haveNext;
};

#endif

// END of PwmSound.h

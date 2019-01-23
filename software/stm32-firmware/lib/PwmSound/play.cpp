/******************************************************************************
 * File:      play.cpp
 * Author:    Paul Griffith
 * Created:   28 Mar 2014
 * Last Edit: see below
 * Version:   see below
 *
 * Description:
 * Play melody from music data written in Music Macro Language (MML) format.
 * Part of PwmSound class.
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
 * 0.00 28Mar14 PG  File created.
 * 1.00 06May14 PG  Initial release.
 *
 ******************************************************************************/
/*
 * Music Macro Language (MML) is a music description language used in sequencing
 * music on computer and video game systems. For further details refer to the
 * Wikipedia article of the same name.
 *
 * There are many dialects of MML. The one used here is essentially that of
 * the PLAY statement from Microsoft GW-BASIC. The original Microsoft
 * documentation is available online by following the References from the
 * Wikipedia GW-BASIC article. The MML data format is described below:
 *
 * A-G  Play note using the letter names of the scale. The letter may be
 *      followed by either a # or + for a sharp, or a - for a flat. Any note
 *      letter followed by a #, + or - must refer to a black key on a piano.
 *
 * K    Keyboard. Stops play() polling the PC keyboard during playback.
 *      If K is not found, any PC keystroke will stop playback.
 *
 * Ln   Sets the length of each note. n is a decimal number between 1 and 64
 *      L1 is a whole note (semi-breve), L4 is a quarter note (crotchet) and
 *      so on. The L value persists until the next L command is encountered.
 *
 *      A length number may also follow a note letter name to change the length
 *      for that note only. For example, D8 is equivalent to L8D.
 *
 * ML   Music Legato. The note is played the for the full length of its
 *      specified time. There is no rest between notes.
 *
 * MN   Music Normal. The note is played for 7/8 of its specified time, and
 *      1/8 of the specified time is a rest between notes. This is the default.
 *
 * MS   Music Staccato. The note is played for 3/4 of its specified time, and
 *      1/4 of the specified time is a rest between notes.
 *
 * Nn   Play note. n is a decimal number between 0 and 84. n = 1 is the lowest
 *      note and n = 84 is the highest note. n set to 0 indicates a rest. N can
 *      be used as an alternative to defining a note by an octave and a letter.
 *      For example, N37 = middle C.
 *
 * On   Sets the current octave. n is a decimal number between 0 and 6. The
 *      default octave is 4. Middle C starts octave 3, i.e. O3C = middle C.
 *
 *      Note: The above convention differs from standard piano octave numbering
 *            where middle C starts octave 4, i.e. O4C = middle C.
 *
 * Pn   Pause, or rest, for a length defined by n. P works in the same way as
 *      the L command above. For example, P2 = a half rest.
 *
 * Qn   Sets the tonal quality (timbre). n is a decimal number between 1 and 4.
 *      It sets the PWM duty cycle to n / 8, (i.e. 12.5%, 25%, 37.5% or 50%).
 *      The default is 4 (50% duty cycle).
 *
 * Rn   Rest, for a length defined by n. Alternative form of the P command.
 *
 * Tn   Sets the tempo (the pace at which the music plays) in beats per minute.
 *      n is a decimal number between 32 and 255. The default tempo is 120. One
 *      beat corresponds to a quarter note (L4).
 *
 * .    Dot. A dot can follow a letter note or a pause. It extends the duration
 *      of the note or pause by half (to 150%). More than one dot may be used.
 *
 *      Note: The Microsoft documentation states that multiple dots extend the
 *            duration as follows:
 *            2 dots = 1.5 ^ 2 = 225%, 3 dots = 1.5 ^ 3 = 337.5%.
 *            This differs from standard musical notation where multiple dots
 *            provide successively smaller extensions as follows:
 *            2 dots = 1 + 1/2 + 1/4 = 175%.
 *            3 dots = 1 + 1/2 + 1/4 + 1/8 = 187.5%.
 *
 *            The standard notation extensions are used here.
 *
 * >    Play the following note in the next higher octave. For example,
 *      O3C >D E is equivalent to O3C O4D O3E.
 *
 * <    Play the following note in the next lower octave. For example,
 *      O3C <D E is equivalent to O3C O2D O3E.
 *
 *      Note: Some dialects of MML appear to treat < and > as persistent
 *            commands that affect all following notes.
 *
 *      Note: The Microsoft documentation does not say whether or not < and >
 *            should act on notes specified by number, such as N37. In this
 *            implementation it does, so >N37 = N49.
 *
 * : #  Treat remainder of line as a comment. Note: line must end with '\n'.
 *
 *      Note: Some dialects of MML use # to start a comment. We accept either.
 *
 * White space and line endings (CR, LF) are ignored (except in comments).
 *
 * Note: The play() function has a second parameter which supports some MML
 *       dialect alternatives. Refer to the comments below for details.
 */

#include "mbed.h"
#include "PwmSound.h"
#include "ctype.h"

extern Serial pc;	//for debug, comment out of not needed

 // Standard note pitches in Hz
// From Wikipedia: http://en.wikipedia.org/wiki/Scientific_pitch_notation
 // First entry is a dummy, real note numbers start at 1
 // Seven octaves, twelve notes per octave
 // C, C#, D, D#, E, F, F#, G, G#, A, A#, B
 // Middle C (261.63Hz) is element 37

 float notePitches[1+84] = {
     1.0,												//dummy
     32.703, 34.648, 36.708, 38.891, 41.203, 43.654,	//first octave
     46.249, 48.999, 51.913, 55.000, 58.270, 61.735,
     65.406, 69.296, 73.416, 77.782, 82.407, 87.307,	//second octave
     92.499, 97.999, 103.83, 110.00, 116.54, 123.47,
     130.81, 138.59, 146.83, 155.56, 164.81, 174.61,	//third octave
     185.00, 196.00, 207.65, 220.00, 233.08, 246.94,
     261.63, 277.18, 293.66, 311.13, 329.63, 349.23,	//fourth octave
     369.99, 392.00, 415.30, 440.00, 466.16, 493.88,
     523.25, 554.37, 587.33, 622.25, 659.26, 698.46,	//fifth octave
     739.99, 783.99, 830.61, 880.00, 932.33, 987.77,
     1046.5, 1108.7, 1174.7, 1244.5, 1318.5, 1396.9,	//sixth octave
     1480.0, 1568.0, 1661.2, 1760.0, 1864.7, 1975.5,
     2093.0, 2217.5, 2349.3, 2489.0, 2637.0, 2793.8,	//seventh octave
     2960.0, 3136.0, 3322.4, 3520.0, 3729.3, 3951.1,
 };

// Note numbers within octave for notes A - G (white keys on piano)

int notes[7] = { 10, 12, 1, 3, 5, 6, 8 };   //C is first note = 1

// Allowable note modifiers for notes A - G

int flats[7] = { -1, -1, 0, -1, -1, 0, -1 };    //not C or F
int sharps[7] = {1, 0, 1, 1, 0, 1, 1 };         //not B or E 

// Play a melody from music data written in MML format.
//
// Parameters:
//    m - pointer to string containing music data
//    options (default 0) - support for different dialects of MML
//      bit 0 (1) - standard octave numbering
//                  0: octaves range from 0 to 6, middle C starts octave 3
//                  1: octaves range from 1 to 7, middle C starts octave 4
//      bit 1 (2) - stickyShift
//                  0: < and > act on next note only
//                  1: < and > act on all following notes
//      bit 2 (4) - longDots
//                  0: standard dot extensions (i.e. 150%, 175%, 187.5%)
//                  1: GW-BASIC dot extensions (i.e. 150%, 225%, 337.5%)
// Returns: 0 if no error in input, otherwise position of offending character

int PwmSound::play(const char* m, int options) {
	bool run = true, kbdPoll = true;
    char c, c1;
    int n, n1, n2;

	bool stdOctNum = (options & 1) ? true : false;	//options bits
    bool stickyShift = (options & 2) ? true : false;
    bool longDots = (options & 4) ? true : false;

    _octave = 4;    //set defaults
    _shift = 0;
    _tempo = 120;
    _length = 4;
    _1dot = 1.5;
    _2dots = (longDots == true) ? 2.25 : 1.75;
    _3dots = (longDots == true) ? 3.375 : 1.875;
    _style = 7;
    _dutyCycle = 0.5;
    _mp = m;
    _haveNext = false;
    //pc.putc('[');
    while (run) {
        if (kbdPoll && pc.readable()) {
            pc.getc();
            break;
        }
        c = _getChar();  //read next char in input stream
        //pc.putc(c);
        switch (c) {
            case 'A':   //specify note by letter (and modifier)
            case 'B':
            case 'C':
            case 'D':
            case 'E':
            case 'F':
            case 'G':
                if (stdOctNum) {
                	n = ((_octave - 1) * 12) + notes[c - 'A'];
                } else {
                	n = (_octave * 12) + notes[c - 'A'];
                }
                c1 = _nextChar();                       //optional modifier
                if (c1 == '-' || c1 == '+' || c1 == '#') {
                    c1 = _getChar();
                    n += (c1 == '-') ? flats[c - 'A'] : sharps[c - 'A'];
                }
                n += _shift * 12;
                if (!stickyShift) {
                	_shift = 0;
                }
                n1 = _getNumber();		//optional length number
                n2 = _getDots();        //optional dots
                if (n1 == 0) {
                    _note(n, _length, n2);
                } else {
                    _note(n, n1, n2);
                }
                break;

            case 'K':
                kbdPoll = false;
                break;

            case 'L':   //set note length
                n = _getNumber();
                if (n >= 1 && n <= 64) {
                    _length = n;
                }
                break;

            case 'M':   //set music style (proportion of note length played)
                switch (_getChar() ) {
                    case 'L':		//legato
                        _style = 8;
                        break;

                    case 'N':		//normal
                        _style = 7;
                        break;

                    case 'S':		//staccato
                        _style = 6;
                        break;
                }
                break;

            case 'N':   //specify note by number
                n = _getNumber();
                n += _shift * 12;		//not really sure about this
                if (!stickyShift) {
                	_shift = 0;
                }
                _note(n, _length);
                break;

            case 'O':   //set octave
                n = _getNumber();
                if (stdOctNum) {
					if (n >= 1 && n <= 7) {
						_octave = n;
					}
                } else {
					if (n >= 0 && n <= 6) {
						_octave = n;
					}
                }
                break;

            case 'P':   //pause or rest
            case 'R':
                n1 = _getNumber();		//optional length number
                n2 = _getDots();        //optional dots
                if (n1 == 0) {
                    _note(0, _length, n2);
                } else {
                    _note(0, n1, n2);
                }
                break;

            case 'Q':   //set timbre
                n = _getNumber();
                if (n >= 1 && n <= 4) {
                	_dutyCycle = n / 8.0;
                }
                break;

            case 'T':   //set tempo
                n = _getNumber();
                if ( n>= 32 && n <= 255)  {
                	_tempo = n;
                }
                break;

            case '<':	//move down an octave
                _shift--;
                break;

            case '>':	//move up an octave
                _shift++;
                break;

            case ':':		//comment to end of line
            case '#':
            	while (_getChar() != '\n') ;
            	break;

            case ' ':		//skip over white space and line endings
            case '\t':
            case '\r':
            case '\n':
            	break;

            case '\0':		//end of string
                run = false;
                break;

            default:		//abort on invalid characters
            	_pin = 0.0;
            	return(int (_mp - m) );	//return position of error
        }
    }   //end of while
    //pc.putc(']');
    _pin = 0.0;
    return 0;
}

// Play a musical note on output pin
//
// Parameters:
//    number - 0 = rest, notes from 1 to 84, middle C (262Hz) = 37
//    length - duration of note (1-64): 1 = whole note (semibreve)
//                                      2 = half note (minim)
//                                      4 = quarter note (crotchet) = 1 beat
//                                      8 = eighth note (quaver)
//                                      etc
//    dots - length extension (0-3, default 0)
// Returns: nothing

void PwmSound::_note(int number, int length, int dots) {
    float duration, play, rest;

    if (number < 1 || number > 84) {    //convert bad note to a rest
        number = 0;
    }

    duration = 240.0 / (_tempo * length);
    if (dots == 1) {
        duration *= _1dot;
    } else if (dots == 2) {
        duration *= _2dots;
    } else if (dots == 3) {
        duration *= _3dots;
    }
    play = duration * _style / 8.0;
    rest = duration * (8 - _style) / 8.0;

    if (number > 0) {
        _pin.period(1.0 / notePitches[number]);
        _pin = _dutyCycle;
    }
    wait(play);
    _pin = 0.0;
    wait(rest);
}

// Read next character in input string
//
// Parameters: none
// Returns: next character

char PwmSound::_getChar(void) {
	if (_haveNext) {
		_haveNext = false;
		return _nextCh;
	} else {
		return *_mp++;
	}
}

// Examine next character in input string without consuming it
//
// Parameters: none
// Returns: next character

char PwmSound::_nextChar(void) {
	if (!_haveNext) {
		_nextCh = *_mp++;
		_haveNext = true;
	}
	return _nextCh;
}

// Read a variable length number from input string
//
// Parameters: none
// Returns: number

int PwmSound::_getNumber(void) {
    int n = 0;

    while (isdigit(_nextChar()) ) {
        n *= 10;
        n += _getChar() - '0';
    }
    return n;
}

// Read variable number of dots from input string
//
// Parameters: none
// Returns: number of dots

int PwmSound::_getDots(void) {
    int n = 0;

    while (_nextChar() == '.') {
        _getChar();
        n++;
    }
    return n;
}

// END of play.cpp

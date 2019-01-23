#include "Keypad.h"
#include <ticker_api.h>
#include <Timer.h>

void Keypad::_cbRow0Rise()
{
    _checkIndex(0, _rows[0]);
}

void Keypad::_cbRow1Rise()
{
    _checkIndex(1, _rows[1]);
}

void Keypad::_cbRow2Rise()
{
    _checkIndex(2, _rows[2]);
}

void Keypad::_cbRow3Rise()
{
    _checkIndex(3, _rows[3]);
}

void Keypad::_cbRowFall() {
}

Keypad::Keypad(PinName r0, PinName r1, PinName r2, PinName r3, PinName c0, PinName c1, PinName c2, int debounce_ms)
    : _row0(r0, PullDown), _row1(r1, PullDown), _row2(r2, PullDown), _row3(r3, PullDown)
    , _col0(c0), _col1(c1), _col2(c2)
    , _rows { &_row0, &_row1, &_row2, &_row3 }
    , _cols { &_col0, &_col1, &_col2 }
{
    _nRow = 4;
    _nCol = 3;

    _row0.rise(callback(this, &Keypad::_cbRow0Rise));
    _row1.rise(callback(this, &Keypad::_cbRow1Rise));
    _row2.rise(callback(this, &Keypad::_cbRow2Rise));
    _row3.rise(callback(this, &Keypad::_cbRow3Rise));

    // _row0.fall(callback(this, &Keypad::_cbRowFall));
    // _row1.fall(callback(this, &Keypad::_cbRowFall));
    // _row2.fall(callback(this, &Keypad::_cbRowFall));
    // _row3.fall(callback(this, &Keypad::_cbRowFall));

    _debounce = debounce_ms;
    _debounceTimer.start();
}

Keypad::~Keypad()
{
}

void Keypad::start()
{
    for (int i = 0; i < _nCol; i++)
        _cols[i]->write(1);
}

void Keypad::stop()
{
    for (int i = 0; i < _nCol; i++)
        _cols[i++]->write(0);
}

void Keypad::attach(uint32_t (*fptr)(uint32_t index))
{
    _callback.attach(fptr);
}

void Keypad::_checkIndex(int row, InterruptIn *therow)
{
    if (_debounceTimer.read_ms() < _debounce)
        return;
    _debounceTimer.reset();

    if (therow->read() == 0)
        return;

    int c;
    for (c = 0; c < _nCol; c++)
    {
        _cols[c]->write(0); // de-energize the column
        if (therow->read() == 0)
        {
            break;
        }
    }

    if (c < _nCol) {
        int index = row * _nCol + c;
        _callback.call(index);
    }

    start(); // Re-energize all columns
}

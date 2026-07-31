#include "stream_reassembler.hh"
#include <algorithm>
#include <string>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) 
    : _output(capacity) 
    , _capacity(capacity) 
    , _unassembled_start(0) 
    , _unassembled_size(0) 
    , _eof(false)
    , _eof_index(0)
    , _buffer(capacity, '\0')
    , _filled(capacity, false)
    {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
        _eof_index = index + data.size();
    }
    if (_capacity == 0) {
        if (_eof && _unassembled_start == _eof_index) _output.end_input();
        return ;
    }
    const size_t first_unacceptable = _unassembled_start + _output.remaining_capacity();
    const size_t data_end = index + data.size();
    const size_t start = max(index, _unassembled_start);
    const size_t end = min(data_end, first_unacceptable);
    if (start < end) {
        for (size_t pos = start; pos < end; ++pos) {
            const size_t offset = pos % _capacity;
            if (!_filled[offset]) {
                _buffer[offset] = data[pos - index];
                _filled[offset] = true;
                ++_unassembled_size;
            }
        }
    }
    string assembled;
    while (_filled[_unassembled_start % _capacity]) {
        const size_t offset = _unassembled_start % _capacity;
        assembled.push_back(_buffer[offset]);
        _filled[offset] = false;
        ++_unassembled_start;
        --_unassembled_size;
    }
    if (!assembled.empty()) _output.write(assembled);
    if (_eof && _unassembled_start == _eof_index) _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_size; }

bool StreamReassembler::empty() const { return _unassembled_size == 0; }

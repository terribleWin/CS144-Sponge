#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer_(), capacity_(capacity), end_write_(false), end_read_(false), byte_written_(0), byte_read_(0) {}
size_t ByteStream::write(const string &data) {
    size_t write_count = min(remaining_capacity(), data.size());

    for (size_t i = 0; i < write_count; i++) {
        buffer_.push_back(data[i]);
    }
    byte_written_ += write_count;
    return write_count;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t peek_count = min(len, buffer_.size());
    string result = "";
    result.reserve(peek_count);
    for (size_t i = 0; i<peek_count; i++) {
        result += buffer_[i];
    }
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t pop_count = min(len, buffer_.size());
    for (size_t i = 0; i<pop_count; i++) {
        buffer_.pop_front();
    }
    byte_read_ += pop_count;
 }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string result = peek_output(len);
    pop_output(result.size());
    return result;
}

void ByteStream::end_input() {end_write_ = true;}

bool ByteStream::input_ended() const { return end_write_; }

size_t ByteStream::buffer_size() const { return buffer_.size(); }

bool ByteStream::buffer_empty() const { return buffer_.empty(); }

bool ByteStream::eof() const { return end_write_ && buffer_.empty(); }

size_t ByteStream::bytes_written() const { return byte_written_; }

size_t ByteStream::bytes_read() const { return byte_read_; }

size_t ByteStream::remaining_capacity() const { return capacity_ - buffer_.size(); }

#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <algorithm>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _rto(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

uint64_t TCPSender::_window_space() const {
    //! A zero-size window is treated as one byte, so that the sender can probe a closed window.
    const uint64_t window_size = _receiver_window_size == 0 ? 1 : _receiver_window_size;
    const uint64_t in_flight = _bytes_in_flight;
    return window_size > in_flight ? window_size - in_flight : 0;
}

void TCPSender::fill_window() {
    while (not _fin_sent) {
        const bool need_syn = not _syn_sent;
        const uint64_t window_space = _window_space();
        //! The SYN occupies one sequence number of the window, and so does the FIN.
        const uint64_t data_space = need_syn ? (window_space > 0 ? window_space - 1 : 0) : window_space;
        const size_t payload_size = min({static_cast<size_t>(TCPConfig::MAX_PAYLOAD_SIZE),
                                         static_cast<size_t>(data_space),
                                         _stream.buffer_size()});
        //! Beware: ByteStream::eof() means "input ended AND everything has been read out", so it is still
        //! false while the data is waiting to be sent. Use input_ended() and check that this segment is the
        //! one that empties the stream. The FIN may only be piggybacked if it also fits in the window
        //! (a FIN consumes one sequence number, so it cannot be added if the window is exactly full).
        const bool need_fin = _stream.input_ended() and payload_size == _stream.buffer_size() and
                              data_space - payload_size > 0;
        if (not need_syn and payload_size == 0 and not need_fin) {
            //! The window is full (or closed), or there is simply nothing left to send.
            break;
        }
        // Create a segment with the appropriate flags and payload, and send it.
        TCPSegment seg;
        seg.header().syn = need_syn;
        seg.header().fin = need_fin;
        seg.payload() = Buffer{_stream.read(payload_size)};
        _send_segment(seg);

        _syn_sent = _syn_sent or need_syn;
        _fin_sent = need_fin;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (!_ack_valid(abs_ackno)) return ;
    //! Set a flag to check if any segment has been removed meaning a new ack
    //! If this flag is true, the timer should be reset.
    //! If this flag is false, the timer should be incremented.
    bool removed_segments = false;
    _receiver_window_size = window_size;
    while (!_segments_outstanding.empty()) {
        TCPSegment& seg = _segments_outstanding.front();
        uint64_t seg_start = unwrap(seg.header().seqno, _isn, _next_seqno);
        uint64_t seg_end = seg_start + seg.length_in_sequence_space();
        if (seg_end <= abs_ackno) {
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_outstanding.pop();
            removed_segments = true;
        } else {
            break;
        }
    }
    if (removed_segments) {
       _rto = _initial_retransmission_timeout;
       _consecutive_retransmissions = 0;
       if (!_segments_outstanding.empty()) {
            _timer_running = true;
            _time_elapsed = 0;
       } else {
            _timer_running = false;
       }
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    if (_timer_running) {
        _time_elapsed += ms_since_last_tick;
    }
    //！ Judge whether the timer has expired
    if (!_timer_running || _time_elapsed < _rto) return ;
    //! Timer has expired, retransmit the earliest outstanding segment
    if (!_segments_outstanding.empty()) {
        TCPSegment& seg = _segments_outstanding.front();
        _segments_out.push(seg);
        if(_receiver_window_size > 0 || _segments_outstanding.front().header().syn) {
            _consecutive_retransmissions++;
            _rto <<= 1;
        }
    }
    //! Reset the timer after retransmission
    _time_elapsed = 0;
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    //! Create an empty segment
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

bool TCPSender::_ack_valid(uint64_t abs_ackno) {
    //! Ackno isn't valid if it is greater than the next sequence number to be sent
    if (abs_ackno > _next_seqno) return false;
    //! Ackno isn't valid if it is less than the sequence number of the earliest outstanding segment
    if (!_segments_outstanding.empty()) {
        uint64_t earliest_unacked_seqno = unwrap(_segments_outstanding.front().header().seqno, _isn, _next_seqno);
        if (abs_ackno < earliest_unacked_seqno) return false;
    }
    return true;
}

void TCPSender::_send_segment(TCPSegment& seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    //! If the segment isn't empty, it should be added to the outstanding segments queue.
    if (seg.length_in_sequence_space() > 0){
        _segments_outstanding.push(seg);
    }
    //! Start the timer 
    if (!_timer_running){
        _timer_running = true;
        _time_elapsed = 0;
    }
}
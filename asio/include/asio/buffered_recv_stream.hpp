//
// buffered_recv_stream.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003 Christopher M. Kohlhoff (chris@kohlhoff.com)
//
// Permission to use, copy, modify, distribute and sell this software and its
// documentation for any purpose is hereby granted without fee, provided that
// the above copyright notice appears in all copies and that both the copyright
// notice and this permission notice appear in supporting documentation. This
// software is provided "as is" without express or implied warranty, and with
// no claim as to its suitability for any purpose.
//

#ifndef ASIO_BUFFERED_RECV_STREAM_HPP
#define ASIO_BUFFERED_RECV_STREAM_HPP

#include "asio/detail/push_options.hpp"

#include "asio/detail/push_options.hpp"
#include <cstring>
#include <boost/noncopyable.hpp>
#include <boost/type_traits.hpp>
#include "asio/detail/pop_options.hpp"

#include "asio/null_completion_context.hpp"
#include "asio/detail/bind_handler.hpp"

namespace asio {

/// The buffered_recv_stream class template can be used to add buffering to the
/// recv-related operations of a stream.
template <typename Next_Layer, int Buffer_Size = 8192>
class buffered_recv_stream
  : private boost::noncopyable
{
public:
  /// Construct, passing the specified demuxer to initialise the next layer.
  template <typename Arg>
  explicit buffered_recv_stream(Arg& a)
    : next_layer_(a),
      bytes_in_buffer_(0),
      read_pos_(0)
  {
  }

  /// The type of the next layer.
  typedef typename boost::remove_reference<Next_Layer>::type next_layer_type;

  /// Get a reference to the next layer.
  next_layer_type& next_layer()
  {
    return next_layer_;
  }

  /// The type of the lowest layer.
  typedef typename next_layer_type::lowest_layer_type lowest_layer_type;

  /// Get a reference to the lowest layer.
  lowest_layer_type& lowest_layer()
  {
    return next_layer_.lowest_layer();
  }

  /// The demuxer type for this asynchronous type.
  typedef typename next_layer_type::demuxer_type demuxer_type;

  /// Get the demuxer associated with the asynchronous object.
  demuxer_type& demuxer()
  {
    return next_layer_.demuxer();
  }

  /// Close the stream.
  void close()
  {
    next_layer_.close();
  }

  /// Send the given data to the peer. Returns the number of bytes sent or 0 if
  /// the stream was closed cleanly. Throws an exception on failure.
  size_t send(const void* data, size_t length)
  {
    return next_layer_.send(data, length);
  }

  /// Start an asynchronous send. The data being sent must be valid for the
  /// lifetime of the asynchronous operation.
  template <typename Handler>
  void async_send(const void* data, size_t length, Handler handler)
  {
    next_layer_.async_send(data, length, handler);
  }

  /// Start an asynchronous send. The data being sent must be valid for the
  /// lifetime of the asynchronous operation.
  template <typename Handler, typename Completion_Context>
  void async_send(const void* data, size_t length, Handler handler,
      Completion_Context& context)
  {
    next_layer_.async_send(data, length, handler, context);
  }

  /// Receive some data from the peer. Returns the number of bytes received or
  /// 0 if the stream was closed cleanly. Throws an exception on failure.
  size_t recv(void* data, size_t max_length)
  {
    if (read_pos_ == bytes_in_buffer_)
    {
      bytes_in_buffer_ = next_layer_.recv(buffer_, Buffer_Size);
      read_pos_ = 0;
      if (bytes_in_buffer_ == 0)
        return 0;
    }

    return copy(data, max_length);
  }

  template <typename Handler, typename Completion_Context>
  class recv_handler
  {
  public:
    recv_handler(demuxer_type& demuxer, char* buffer,
        size_t& bytes_in_buffer, size_t& read_pos, void* data,
        size_t max_length, Handler handler, Completion_Context& context)
      : demuxer_(demuxer),
        buffer_(buffer),
        bytes_in_buffer_(bytes_in_buffer),
        read_pos_(read_pos),
        data_(data),
        max_length_(max_length),
        handler_(handler),
        context_(context)
    {
    }

    template <typename Error>
    void operator()(const Error& e, size_t bytes_recvd)
    {
      bytes_in_buffer_ = bytes_recvd;
      read_pos_ = 0;
      if (bytes_in_buffer_ == 0)
      {
        demuxer_.operation_immediate(detail::bind_handler(handler_, e, 0),
            context_);
      }
      else
      {
        using namespace std; // For memcpy.

        size_t bytes_avail = bytes_in_buffer_ - read_pos_;
        size_t length = (max_length_ < bytes_avail)
          ? max_length_ : bytes_avail;
        memcpy(data_, buffer_ + read_pos_, length);
        read_pos_ += length;
        demuxer_.operation_immediate(
            detail::bind_handler(handler_, e, length), context_);
      }
    }

  private:
    demuxer_type& demuxer_;
    char* buffer_;
    size_t& bytes_in_buffer_;
    size_t& read_pos_;
    void* data_;
    size_t max_length_;
    Handler handler_;
    Completion_Context& context_;
  };

  /// Start an asynchronous receive. The buffer for the data being received
  /// must be valid for the lifetime of the asynchronous operation.
  template <typename Handler>
  void async_recv(void* data, size_t max_length, Handler handler)
  {
    if (read_pos_ == bytes_in_buffer_)
    {
      next_layer_.async_recv(buffer_, Buffer_Size,
          recv_handler<Handler, null_completion_context>(demuxer(), buffer_,
            bytes_in_buffer_, read_pos_, data, max_length, handler,
            null_completion_context::instance()));
    }
    else
    {
      size_t length = copy(data, max_length);
      next_layer_.demuxer().operation_immediate(
          detail::bind_handler(handler, 0, length));
    }
  }

  /// Start an asynchronous receive. The buffer for the data being received
  /// must be valid for the lifetime of the asynchronous operation.
  template <typename Handler, typename Completion_Context>
  void async_recv(void* data, size_t max_length, Handler handler,
      Completion_Context& context)
  {
    if (read_pos_ == bytes_in_buffer_)
    {
      next_layer_.async_recv(buffer_, Buffer_Size,
          recv_handler<Handler, Completion_Context>(demuxer(), buffer_,
            bytes_in_buffer_, read_pos_, data, max_length, handler, context));
    }
    else
    {
      size_t length = copy(data, max_length);
      next_layer_.demuxer().operation_immediate(
          detail::bind_handler(handler, 0, length), context);
    }
  }

private:
  /// Copy data out of the internal buffer to the specified target buffer.
  /// Returns the number of bytes copied.
  size_t copy(void* data, size_t max_length)
  {
    using namespace std; // For memcpy.

    size_t bytes_avail = bytes_in_buffer_ - read_pos_;
    size_t length = (max_length < bytes_avail) ? max_length : bytes_avail;
    memcpy(data, buffer_ + read_pos_, length);
    read_pos_ += length;

    return length;
  }

  /// The next layer.
  Next_Layer next_layer_;

  // The data in the buffer.
  char buffer_[Buffer_Size];

  /// The amount of data currently in the buffer.
  size_t bytes_in_buffer_;

  /// The current read position in the buffer.
  size_t read_pos_;
};

} // namespace asio

#include "asio/detail/pop_options.hpp"

#endif // ASIO_BUFFERED_RECV_STREAM_HPP

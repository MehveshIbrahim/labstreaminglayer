//
// ssl/detail/read_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.lslboost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_SSL_DETAIL_READ_OP_HPP
#define BOOST_ASIO_SSL_DETAIL_READ_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <lslboost/asio/detail/config.hpp>

#if !defined(BOOST_ASIO_ENABLE_OLD_SSL)
# include <lslboost/asio/detail/buffer_sequence_adapter.hpp>
# include <lslboost/asio/ssl/detail/engine.hpp>
#endif // !defined(BOOST_ASIO_ENABLE_OLD_SSL)

#include <lslboost/asio/detail/push_options.hpp>

namespace lslboost {
namespace asio {
namespace ssl {
namespace detail {

#if !defined(BOOST_ASIO_ENABLE_OLD_SSL)

template <typename MutableBufferSequence>
class read_op
{
public:
  read_op(const MutableBufferSequence& buffers)
    : buffers_(buffers)
  {
  }

  engine::want operator()(engine& eng,
      lslboost::system::error_code& ec,
      std::size_t& bytes_transferred) const
  {
    lslboost::asio::mutable_buffer buffer =
      lslboost::asio::detail::buffer_sequence_adapter<lslboost::asio::mutable_buffer,
        MutableBufferSequence>::first(buffers_);

    return eng.read(buffer, ec, bytes_transferred);
  }

  template <typename Handler>
  void call_handler(Handler& handler,
      const lslboost::system::error_code& ec,
      const std::size_t& bytes_transferred) const
  {
    handler(ec, bytes_transferred);
  }

private:
  MutableBufferSequence buffers_;
};

#endif // !defined(BOOST_ASIO_ENABLE_OLD_SSL)

} // namespace detail
} // namespace ssl
} // namespace asio
} // namespace lslboost

#include <lslboost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_SSL_DETAIL_READ_OP_HPP
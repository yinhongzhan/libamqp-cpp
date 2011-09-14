#include "connection.h"
#include "amqp_sasl.h"
#include "frame.h"
#include "methods.gen.h"

#include <boost/array.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/shared_ptr.hpp>

#include <algorithm>
#include <istream>
#include <ostream>

using boost::asio::ip::tcp;

namespace amqpp
{

connection::connection(const std::string& host, int port, const std::string& vhost,
		const std::string& username, const std::string& password) :
	m_host(host), m_port(port), m_vhost(vhost), m_username(username), m_password(password)
{
}

connection::~connection()
{
}

void connection::connect()
{
  tcp::resolver resolver(m_io_service);
  tcp::resolver::query query("localhost", boost::lexical_cast<std::string>(5672));

  tcp::iostream io(query);
  io.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);

  static const boost::array<char, 8> handshake = { { 'A', 'M', 'Q', 'P', 0, 0, 9, 1 } };
  io.write(handshake.data(), handshake.size());

  char resp = io.peek();

  if (resp != detail::frame::METHOD_TYPE)
  {
      // connection failure
  }

  boost::shared_ptr<detail::frame> fr = detail::frame::read_frame(io);

  boost::iostreams::stream_buffer<boost::iostreams::array_source> frame_streambuf(boost::asio::buffer_cast<char*>(fr->get_payload_data()), fr->get_payload_size());

  std::istream is(&frame_streambuf);
  is.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);

  boost::shared_ptr<detail::method> method = detail::method::read(is);

  boost::shared_ptr<methods::connection::start> start_method = boost::shared_dynamic_cast<methods::connection::start>(method);

  if (0 != start_method->get_version_major() ||
      9 != start_method->get_version_minor())
  {
    // fail
  }

  methods::connection::start_ok start_ok;
  std::string mechanism = sasl::select_sasl_mechanism(start_method->get_mechanisms());
  start_ok.set_mechanism(mechanism);
  start_ok.set_response(sasl::get_sasl_response(mechanism, m_username, m_password));

  boost::asio::streambuf sb;
  std::ostream os(&sb);
  start_ok.write(os);

  detail::frame out_frame(detail::frame::METHOD_TYPE, 0, sb.data());

}

} // namespace amqpp

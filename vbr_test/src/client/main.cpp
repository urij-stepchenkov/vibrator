/* main.cpp */
static const char rcsid[] = "$Id$";

#include <utility>
#include <iostream>
#include <boost/format.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

namespace bs = boost::system;
namespace ba = boost::asio;
namespace bpo = boost::program_options;

//------------------------------------------------------------------------------------------
/** Client implementation. */
class Client
{
public:
  
  /** Options of the client. */
  struct Options
  {
    Options():
      port(VBR_DEF_SERVER_PORT),
      addr_str(VBR_DEF_SERVER_ADDR)
    {/* nothing to do */}
    
    int port; /**< Server port. */
    std::string addr_str; /**< Server address. */
  };
  
  /** Default constructor. */
  Client():
    m_posix_stream(ms_service, ::dup(STDIN_FILENO)),
    m_ask_msg_flag(false)
  {/* nothing to do */}
  
  /** Connect to server. 
      @param opts  Client options. */
  void connect(const Options &opts)
  {
    bs::error_code ec;
    
    std::cout << "VBR Client " << VBR_VER_MAJ << "." << VBR_VER_MIN << std::endl;
    
    // connect to server
    ba::ip::tcp::endpoint ep(ba::ip::address::from_string(opts.addr_str), opts.port);
    mp_socket = new ba::ip::tcp::socket(ms_service);
    mp_socket->connect(ep);
    
    std::cout << "Connected to " << opts.addr_str << ":" << opts.port << std::endl;
  }
  
  /** Start connected client session.
      (it would be possible to unite connect() and run()). */
  void run()
  {
    std::cout << "> " << std::flush;
    
    // begin async reading from the socket
    ba::async_read(*mp_socket, ba::buffer(m_read_buf, VBR_MSG_BUFF_SIZE), 
		   boost::bind(&Client::read_cb, this,
			       boost::asio::placeholders::error,
			       boost::asio::placeholders::bytes_transferred));
    
    // begin type processing
    ba::async_read_until(m_posix_stream, ms_key_buf, '\n', 
    			 boost::bind(&Client::type_cb, this,
    				     boost::asio::placeholders::error,
    				     boost::asio::placeholders::bytes_transferred));
    
    // run the service
    ms_service.run();
  }
  
private:
  
  /** Type processing callback. */
  void type_cb(const boost::system::error_code& error, 
	       size_t bytes_transferred)
  {
    if (error)
      {
	ms_service.stop();
	return;
      }
    
    // get current line
    std::istream is(&ms_key_buf);
    std::string str;
    std::getline(is, str);
    
    // asc message mode
    if (m_ask_msg_flag)
      {
	ba::async_write(*mp_socket, ba::buffer(str),
			boost::bind(&Client::write_cb, this,
				    boost::asio::placeholders::error,
				    boost::asio::placeholders::bytes_transferred));
      }
    // asc command mode
    else
      {
	boost::algorithm::trim(str);
	if (str == "e")
	  {
	    ms_service.stop();
	    return;
	  }
	else if (str == "s")
	  {
	    std::cout << "msg> " << std::flush;
	    
	    // enable ask message mode
	    m_ask_msg_flag = true;
	    	    
	    // continue typing
	    ms_key_buf.consume(bytes_transferred);
	    ba::async_read_until(m_posix_stream, ms_key_buf, '\n', 
				 boost::bind(&Client::type_cb, this,
					     boost::asio::placeholders::error,
					     boost::asio::placeholders::bytes_transferred));
	  }
	else
	  {
	    std::cout << str << " is unsupported command (e - for exit, s - for send)";
	    std::cout << std::endl;
	    std::cout << "> " << std::flush;
	    
	    // continue typing
	    ms_key_buf.consume(bytes_transferred);
	    ba::async_read_until(m_posix_stream, ms_key_buf, '\n', 
				 boost::bind(&Client::type_cb, this,
					     boost::asio::placeholders::error,
					     boost::asio::placeholders::bytes_transferred));
	  }
      }
  }
  
  /** Write message callback. */
  void write_cb(const boost::system::error_code& err,
		std::size_t /*bytes_transferred*/)
  {
    if (err)
      {
	std::cout << "Client::write_cb: error, " << err << std::endl;
	ms_service.stop();
	return;
      }
    
    // disable ask message mode, go to ask command mode
    m_ask_msg_flag = false;
    std::cout << "> " << std::flush;
    
    // continue typing
    ms_key_buf.consume(ms_key_buf.size() + 1);
    ba::async_read_until(m_posix_stream, ms_key_buf, '\n', 
    			 boost::bind(&Client::type_cb, this,
    				     boost::asio::placeholders::error,
    				     boost::asio::placeholders::bytes_transferred));
  }
  
  /** Read messages from socket callback. */
  void read_cb(const boost::system::error_code& err, 
	       std::size_t /*bytes_transferred*/)
  {
    if (err)
      {
	std::cout << "Client::read_cb: error," << err << std::endl;
	ms_service.stop();
	return;
      }
    
    // show obtained message
    std::cout << "recv: " <<  m_read_buf << std::endl; 
    std::cout <<  ">" << std::flush;
    
    // continue reading
    ba::async_read(*mp_socket, ba::buffer(m_read_buf, VBR_MSG_BUFF_SIZE), 
		   boost::bind(&Client::read_cb, this,
			       boost::asio::placeholders::error,
			       boost::asio::placeholders::bytes_transferred));
  }
  
  /**< Communication service. */
  static ba::io_service ms_service;

  /**< Socket ptr.  */
  ba::ip::tcp::socket *mp_socket;
  
  /** Reading buffer. */
  char m_read_buf[VBR_MSG_BUFF_SIZE];
  
  /**< std::cin stream. */
  ba::posix::stream_descriptor m_posix_stream;
  
  /**< std::cin stream buffer. */
  ba::streambuf ms_key_buf;
  
  /**< Messages (true) or commands (false) processing mode. */
  bool m_ask_msg_flag;
};

ba::io_service Client::ms_service;

//------------------------------------------------------------------------------------------
/** Cmd line args parser. 
    @return <client options, ready to start flag> 
*/
std::pair<Client::Options, bool> get_client_options(int argc, char **argv)
{
  // initialize program options
  bpo::options_description odsc((boost::format("VBR Client %d.%d")
				 %VBR_VER_MAJ %VBR_VER_MIN).str());
  odsc.add_options()
    ("port", bpo::value<int>(),
     (boost::format("Server port (%d by default)") %VBR_DEF_SERVER_PORT).str().c_str())
    ("addr", bpo::value<std::string>(),
     (boost::format("Server address (%s by default)") %VBR_DEF_SERVER_ADDR).str().c_str())
    ("help", "show this help");
  
  Client::Options copts;
  
  // parse cmd line arguments
  try
    {
      bpo::variables_map vm;
      bpo::store(bpo::parse_command_line(argc, argv, odsc), vm);
      bpo::notify(vm);
      
      // user asc help => show and return not ready options
      if (vm.count("help"))
	{
	  std::cout << odsc << std::endl;
	  return std::make_pair(copts, false);
	}
      
      // get port
      if (vm.count("port")) 
	copts.port = vm["port"].as<int>();
      
      // get server address
      if (vm.count("addr")) 
	copts.addr_str = vm["addr"].as<std::string>();
    }
  catch (std::exception &e)
    {
      // just show help on any parsing trouble
      std::cout << odsc << std::endl;
      return std::make_pair(copts, false);
    }
  
  return std::make_pair(copts, true);
}

//------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  // parse cmd line parameters into client options
  std::pair<Client::Options, bool> copts = get_client_options(argc, argv);
  if (!copts.second)
    return 0;
  
  try
    {
      // connect to server
      Client client;
      client.connect(copts.first);
      
      // run client session
      client.run();
    }
  catch (const bs::system_error& err)
    {
      std::cout << "Failed due to " << err.what() << std::endl;
      return -1;
    }
  
  return 0;
}

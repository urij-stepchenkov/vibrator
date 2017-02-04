/* main.cpp */
static const char rcsid[] = "$Id$";

#include <list>
#include <iostream>
#include <boost/format.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>

namespace ba = boost::asio;
namespace bpo = boost::program_options;
namespace bs = boost::system;

/** Server options description. */
struct ServerOptions
{
  /** Default constructor, not ready options by default. */
  ServerOptions():
    port(VBR_DEF_SERVER_PORT)
  {/* nothing to do */}
  
  int port;   /**< Server port. */
};

//------------------------------------------------------------------------------------------
/** Cmd line args parser. 
    @return <server options, ready to start flag> 
*/
std::pair<ServerOptions, bool> parse_argv(int argc, char **argv)
{
  // initialize program options
  bpo::options_description odsc((boost::format("VBR Server %d.%d")
				 %VBR_VER_MAJ %VBR_VER_MIN).str());
  odsc.add_options()
    ("port", bpo::value<int>(),
     (boost::format("Server port (%d by default)") %VBR_DEF_SERVER_PORT).str().c_str())
    ("help", "show this help");
  
  ServerOptions so;
  try
    {
      // parse cmd line arguments
      bpo::variables_map vm;
      bpo::store(bpo::parse_command_line(argc, argv, odsc), vm);
      bpo::notify(vm);
      
      // user asc help
      if (vm.count("help"))
	{
	  std::cout << odsc << std::endl;
	  return std::make_pair(so, false);
	}
      
      // get port
      if (vm.count("port")) 
	so.port = vm["port"].as<int>();
    }  
  catch (std::exception &e)
    {
      // just print help on any parsing trouble
      std::cout << odsc << std::endl;
      return std::make_pair(so, false);
    }

  return std::make_pair(so, true);
}

//------------------------------------------------------------------------------------------
/** One connecting client communication session. */
class Session
{
public:
  
  /** Make session with I/O service provided. */
  Session(ba::io_service &io_service):
    m_is_actual(false),
    m_socket(io_service)
  {
    memset(m_buf, 0, VBR_MSG_BUFF_SIZE);
  }
  
  /** Destroy session. */
  ~Session()
  {
    m_socket.close();
  }
  
  /** Start communication session. Session become actual and ready to send/recv. */
  void start()
  {
    m_session_list.push_back(this);
    m_is_actual = true;
    m_socket.async_read_some(boost::asio::buffer(m_buf, VBR_MSG_BUFF_SIZE),
			     boost::bind(&Session::read_cb, this,
					 boost::asio::placeholders::error,
					 boost::asio::placeholders::bytes_transferred));
  }
  
  /** @return socket. */
  ba::ip::tcp::socket& socket()
  {
    return m_socket;
  }
  
  /** @return global sessions list. */
  std::list<Session*>& list() 
  {
    return m_session_list;
  }
  
  /** @return whether session actual or not. */
  bool isActual() const {return m_is_actual;}
  
protected:
  
  /** Callback (handler) for async reading. */
  void read_cb(const boost::system::error_code& error,
	       size_t bytes_transferred)
  {
    if (error)
      {
	std::cout << "Session::read_cb, error, " << this;
	std::cout << ", " << error << ", became inactual" << std::endl;
	
	// mark this session in the list as inactual 
	m_is_actual = false;

	return;
      }
    
    std::cout << "Session::read_cb: " << this << ", " << bytes_transferred << " bytes.";
    std::cout << std::endl;
    
    // send received messages to all clients except this one
    for (std::list<Session*>::iterator it = m_session_list.begin();
    	 it != m_session_list.end(); ++it)
      if (*it != this)
	(*it)->socket().send(ba::buffer(m_buf, VBR_MSG_BUFF_SIZE));
    
    // continue reading
    memset(m_buf, 0, VBR_MSG_BUFF_SIZE);
    m_socket.async_read_some(boost::asio::buffer(m_buf, VBR_MSG_BUFF_SIZE),
			    boost::bind(&Session::read_cb, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
  }

private:
  
  /**< Session actuality flag. */
  bool m_is_actual; 
  
  /**< Socket. */
  ba::ip::tcp::socket m_socket;
  
  /**< Buffer for messages, global for all sessions, 
     we have only one thread. */
  static char m_buf[VBR_MSG_BUFF_SIZE];
  
  /**< Global list of ptrs to sessions created. */
  static std::list<Session*> m_session_list;
};

char Session::m_buf[VBR_MSG_BUFF_SIZE];
std::list<Session*> Session::m_session_list;


//------------------------------------------------------------------------------------------
/** Server, accepts incoming connections. */
class Server
{
public:
  
  /** Access to the only one server instance. */
  static Server& instance(ba::ip::tcp::acceptor &acc)
  {
    static Server *psrv = new Server(acc);
    return *psrv;
  }
  
  /** Start server. */
  void run()
  {
    ba::io_service &srv = mp_acc->get_io_service();
    
    Session* new_session = new Session(srv);
    mp_acc->async_accept(new_session->socket(),
			 boost::bind(&Server::accept_cb, this, new_session,
				     boost::asio::placeholders::error));
    
    srv.run();
  }
  
protected:
  
  /** Connection accepting callback. */
  void accept_cb(Session *session, 
		 const boost::system::error_code &err)
  {
    if (err)
      {
	std::cout << "Server::accept_cb: error, " << err << std::endl;
	mp_acc->get_io_service().stop();
	return;
      }
    
    // start new session
    session->start();
    
    // remove nonactual sessions from global list
    for (std::list<Session*>::iterator it = session->list().begin();
    	 it != session->list().end(); ++it)
      {
	std::cout << "Server::accept_cb: " << *it << ", actual=";
	std::cout << (*it)->isActual() << std::endl;
	
	if (!(*it)->isActual())
	  {
	    delete *it;
	    *it = NULL;
	  }
      }
    session->list().remove(NULL);
    std::cout << std::endl;
    
    // listening further
    Session *new_session = new Session(mp_acc->get_io_service());
    mp_acc->async_accept(new_session->socket(),
			 boost::bind(&Server::accept_cb, this, new_session,
				     boost::asio::placeholders::error));
  }
  
private:
  
  Server(ba::ip::tcp::acceptor &acc)
  {mp_acc = &acc;}
  
  Server(){}
  
  Server(const Server&){}
  
  void operator=(const Server &);
  
  /**< Ptr to listening acceptor. */
  static ba::ip::tcp::acceptor *mp_acc;
};

ba::ip::tcp::acceptor* Server::mp_acc = NULL;

//------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  // get server options form app args
  std::pair<ServerOptions, bool> so = parse_argv(argc, argv);
  if (!so.second)
    return 0;
  
  std::cout << boost::format("VBR Server %d.%d") %VBR_VER_MAJ %VBR_VER_MIN << std::endl;
  std::cout << boost::format("port: %d") %so.first.port << std::endl;
  
  try
    {
      ba::io_service service;
      ba::ip::tcp::endpoint ep(ba::ip::tcp::v4(), so.first.port); 
      ba::ip::tcp::acceptor acc(service, ep);
      Server::instance(acc).run();
    }
   catch (const bs::system_error& err)
    {
      std::cout << "Failed due to " << err.what() << std::endl;
      return -1;
    }
  
  return 0;
}


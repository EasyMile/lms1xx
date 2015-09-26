#include <algorithm> // copy
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include "lms1xx/lms1xx.hh"

namespace lms1xx {

namespace /* unnamed */ {

/*------------------------------------------------------------------------------------------------*/

// To ignore some values when parsing an istream.
static auto _ = std::string{};

// 1MB of buffer for incoming data.
static constexpr auto maximal_buffer_size = 1048576ul;

// Separator of ASCII commands.
static constexpr auto space = " ";

// Telegram delimiters.
static constexpr auto frame_start = char{0x02};
static constexpr auto frame_end = char{0x03};

///// @todo Make this configurable
//static constexpr auto timeout = std::chrono::seconds{2};

/*------------------------------------------------------------------------------------------------*/

inline
void
build_telegram_impl(std::stringstream& ss)
{
  ss << frame_end;
}

template <typename Arg, typename... Args>
void
build_telegram_impl(std::stringstream& ss, Arg&& arg, Args&&... args)
{
  ss << arg;
  build_telegram_impl(ss, std::forward<Args>(args)...);
}

template <typename... Args>
std::string
build(Args&&... args)
{
  std::stringstream ss;
  ss << frame_start;
  build_telegram_impl(ss, std::forward<Args>(args)...);
  return ss.str();
}

/*------------------------------------------------------------------------------------------------*/

} // namespace unnamed

/*------------------------------------------------------------------------------------------------*/

LMS1xx::LMS1xx()
  : m_io{}
  , m_socket{m_io}
  , m_buffer(maximal_buffer_size)
  , m_timer{m_io}
  , m_connected{false}
{
  m_buffer.prepare(262144); // reserve 256 kB
}

/*------------------------------------------------------------------------------------------------*/

LMS1xx::LMS1xx(const std::string& host, const std::string& port)
  : LMS1xx::LMS1xx{}
{
  connect(host, port);
}

/*------------------------------------------------------------------------------------------------*/

LMS1xx::~LMS1xx()
{
  disconnect();
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::connect(const std::string& host, const std::string& port)
{
	if (not m_connected)
  {
    boost::asio::ip::tcp::resolver resolver{m_io};
    /// @todo Check if sucessful or not
    boost::asio::connect(m_socket, resolver.resolve({host, port}));
    m_connected = true;
	}
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::disconnect()
{
	if (m_connected)
  {
		m_socket.close();
		m_connected = false;
	}
}

/*------------------------------------------------------------------------------------------------*/

bool
LMS1xx::connected()
const noexcept
{
	return m_connected;
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::read()
{
  m_buffer.consume(m_buffer.size()); // reset buffer
  boost::asio::read_until(m_socket, m_buffer, frame_end);

//  auto deadline_reached = false;
//  auto socket_read = false;
//  auto read_ec = boost::system::error_code{};
//  auto timer_ec = boost::system::error_code{};
//
//  m_timer.expires_from_now(timeout);
//  m_timer.async_wait([&](const boost::system::error_code& e)
//                     {
//                       if (e != boost::asio::error::operation_aborted)
//                       {
//                         deadline_reached = true;
//                       }
//                       timer_ec = e;
//                     });
//
//  boost::asio::async_read_until( m_socket, m_buffer, frame_end
//                               , [&](const boost::system::error_code& e, std::size_t)
//                                 {
//                                   socket_read = true;
//                                   read_ec = e;
//                                 });
//
//  while (m_io.run_one())
//  {
//    if (timer_ec == boost::asio::error::operation_aborted)
//    {
//      timer_ec = boost::system::error_code{};
//      continue;
//    }
//
//    if (read_ec)
//    {
//      throw read_ec;
//    }
//
//    if (timer_ec)
//    {
//      throw timer_ec;
//    }
//
//    if (deadline_reached)
//    {
//      throw std::runtime_error{"Timeout"};
//    }
//    else
//    {
//      m_timer.expires_from_now(timeout);
//      m_timer.async_wait([&](const boost::system::error_code& e)
//                         {
//                           if (e != boost::asio::error::operation_aborted)
//                           {
//                             deadline_reached = true;
//                           }
//                           timer_ec = e;
//                         });
//    }
//
//    if (socket_read)
//    {
//      break;
//    }
//  }

  if (*std::istreambuf_iterator<char>{&m_buffer} != frame_start)
  {
    throw std::runtime_error{"Invalid telegram"};
  }
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::write(const std::string& telegram)
{
  boost::asio::write(m_socket, boost::asio::buffer(telegram));
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::write(const char* telegram, std::size_t len)
{
  boost::asio::write(m_socket, boost::asio::buffer(telegram, len));
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::start_measurements()
{
  write(build("sMN", space, "LMCstartmeas"));
  read();
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::stop_measurements()
{
  write(build("sMN", space, "LMCstopmeas"));
  read();
}

/*------------------------------------------------------------------------------------------------*/

device_status
LMS1xx::status()
{
  write(build("sRN", space, "STlms"));
  read();

  auto st = 0;
  std::istream buffer_stream{&m_buffer};
  buffer_stream >> _ >> _ >> st;

  return static_cast<device_status>(st);
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::login()
{
  write(build("sMN", space, "SetAccessMode", space, "03", space, "F4724744"));
  read();
}

/*------------------------------------------------------------------------------------------------*/

scan_configuration
LMS1xx::get_configuration()
{
  write(build("sRN", space, "LMPscancfg"));
  read();

  auto cfg = scan_configuration{};
  std::istream buffer_stream{&m_buffer};
  buffer_stream
    >> _
    >> _
    >> std::hex >> cfg.scaningFrequency
    >> _
    >> std::hex >> cfg.angleResolution
    >> std::hex >> cfg.startAngle
    >> std::hex >> cfg.stopAngle;

	return cfg;
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::set_scan_configuration(const scan_configuration& cfg)
{
  /// @todo Use build_telegram
  char buf[512];
  sprintf(buf, "%c%s %X +1 %X %X %X%c", frame_start, "sMN mLMPsetscancfg",
          cfg.scaningFrequency, cfg.angleResolution, cfg.startAngle,
          cfg.stopAngle, frame_end);

  write(buf, strlen(buf));
  read();
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::set_scan_data_configuration(const scan_data_configuration& cfg)
{
  /// @todo Use build_telegram
	char buf[512];
	sprintf(buf, "%c%s %02X 00 %d %d 0 %02X 00 %d %d 0 %d +%d%c", frame_start,
			"sWN LMDscandatacfg", cfg.outputChannel, cfg.remission ? 1 : 0,
			cfg.resolution, cfg.encoder, cfg.position ? 1 : 0,
			cfg.deviceName ? 1 : 0, cfg.timestamp ? 1 : 0, cfg.outputInterval, frame_end);

  write(buf, strlen(buf));
  read();
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::scan_continous(bool start)
{
  write(build("sEN", space,  "LMDscandata", space, start ? 1 : 0));
  read();
}

/*------------------------------------------------------------------------------------------------*/

boost::optional<scanData>
LMS1xx::getData()
{
  read();
  const auto sz = m_buffer.size();
  auto buf = std::unique_ptr<char[]>(new char[sz]);
  std::copy(std::istreambuf_iterator<char>{&m_buffer}, std::istreambuf_iterator<char>{}, buf.get());
  buf[sz - 1] = '\0';

	char* tok = strtok(buf.get(), " "); //Type of command
	tok = strtok(NULL, " "); //Command
	tok = strtok(NULL, " "); //VersionNumber
	tok = strtok(NULL, " "); //DeviceNumber
	tok = strtok(NULL, " "); //Serial number
	tok = strtok(NULL, " "); //DeviceStatus
	tok = strtok(NULL, " "); //MessageCounter
	tok = strtok(NULL, " "); //ScanCounter
	tok = strtok(NULL, " "); //PowerUpDuration
	tok = strtok(NULL, " "); //TransmissionDuration
	tok = strtok(NULL, " "); //InputStatus
	tok = strtok(NULL, " "); //OutputStatus
	tok = strtok(NULL, " "); //ReservedByteA
	tok = strtok(NULL, " "); //ScanningFrequency
	tok = strtok(NULL, " "); //MeasurementFrequency
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " ");
	tok = strtok(NULL, " "); //NumberEncoders
	int NumberEncoders;
	sscanf(tok, "%d", &NumberEncoders);

  for (auto i = 0; i < NumberEncoders; ++i)
  {
		tok = strtok(NULL, " "); //EncoderPosition
		tok = strtok(NULL, " "); //EncoderSpeed
	}

	tok = strtok(NULL, " "); //NumberChannels16Bit
	int NumberChannels16Bit;
	sscanf(tok, "%d", &NumberChannels16Bit);

  auto data = scanData{};

  for (int i = 0; i < NumberChannels16Bit; i++)
  {
		int type = -1; // 0 DIST1 1 DIST2 2 RSSI1 3 RSSI2
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1"))
    {
			type = 0;
		}
    else if (!strcmp(content, "DIST2"))
    {
			type = 1;
		}
    else if (!strcmp(content, "RSSI1"))
    {
			type = 2;
		}
    else if (!strcmp(content, "RSSI2"))
    {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (type == 0)
    {
			data.dist_len1 = NumberData;
		}
    else if (type == 1)
    {
			data.dist_len2 = NumberData;
		}
    else if (type == 2)
    {
			data.rssi_len1 = NumberData;
		}
    else if (type == 3)
    {
			data.rssi_len2 = NumberData;
		}

		for (auto i = 0; i < NumberData; ++i)
    {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
				data.dist1[i] = dat;
			} else if (type == 1) {
				data.dist2[i] = dat;
			} else if (type == 2) {
				data.rssi1[i] = dat;
			} else if (type == 3) {
				data.rssi2[i] = dat;
			}

		}
	}

	tok = strtok(NULL, " "); //NumberChannels8Bit
	int NumberChannels8Bit;
	sscanf(tok, "%d", &NumberChannels8Bit);

  for (int i = 0; i < NumberChannels8Bit; i++) {
		int type = -1;
		char content[6];
		tok = strtok(NULL, " "); //MeasuredDataContent
		sscanf(tok, "%s", content);
		if (!strcmp(content, "DIST1")) {
			type = 0;
		} else if (!strcmp(content, "DIST2")) {
			type = 1;
		} else if (!strcmp(content, "RSSI1")) {
			type = 2;
		} else if (!strcmp(content, "RSSI2")) {
			type = 3;
		}
		tok = strtok(NULL, " "); //ScalingFactor
		tok = strtok(NULL, " "); //ScalingOffset
		tok = strtok(NULL, " "); //Starting angle
		tok = strtok(NULL, " "); //Angular step width
		tok = strtok(NULL, " "); //NumberData
		int NumberData;
		sscanf(tok, "%X", &NumberData);

		if (type == 0) {
			data.dist_len1 = NumberData;
		} else if (type == 1) {
			data.dist_len2 = NumberData;
		} else if (type == 2) {
			data.rssi_len1 = NumberData;
		} else if (type == 3) {
			data.rssi_len2 = NumberData;
		}
		for (int i = 0; i < NumberData; i++) {
			int dat;
			tok = strtok(NULL, " "); //data
			sscanf(tok, "%X", &dat);

			if (type == 0) {
				data.dist1[i] = dat;
			} else if (type == 1) {
				data.dist2[i] = dat;
			} else if (type == 2) {
				data.rssi1[i] = dat;
			} else if (type == 3) {
				data.rssi2[i] = dat;
			}
		}
	}

  return data;
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::save_configuration()
{
  write(build("sMN", space, "mEEwriteall"));
  read();
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::start_device()
{
  write(build("sMN", space, "Run"));
  read();
}

/*------------------------------------------------------------------------------------------------*/

} // namespace lms1xx

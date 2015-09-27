#include <algorithm>     // copy
#include <iomanip>
#include <memory>        // unique_ptr
#include <sstream>

#include <boost/asio/connect.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include "lms1xx/lms1xx.hh"

namespace lms1xx {

namespace /* unnamed */ {

/*------------------------------------------------------------------------------------------------*/

// To ignore some values when parsing an istream.
static auto _ = std::string{};

// 256 kB of buffer for incoming data.
static constexpr auto maximal_buffer_size = 262144ul;

// Separator of ASCII commands.
static constexpr auto space = " ";

// Telegram delimiters.
static constexpr auto frame_start = char{0x02};
static constexpr auto frame_end   = char{0x03};

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

LMS1xx::LMS1xx(const boost::posix_time::time_duration& timeout)
  : m_io{}
  , m_socket{m_io}
  , m_buffer(maximal_buffer_size)
  , m_timer{m_io}
  , m_connected{false}
  , m_timeout{timeout}
{
  m_buffer.prepare(131072); // reserve 128 kB
  m_timer.expires_at(boost::posix_time::pos_infin);
  check_timer();
}

/*------------------------------------------------------------------------------------------------*/

LMS1xx::LMS1xx( const std::string& host, const std::string& port
              , const boost::posix_time::time_duration& timeout)
  : LMS1xx::LMS1xx{timeout}
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
LMS1xx::check_timer()
{
  if (m_timer.expires_at() <= boost::asio::deadline_timer::traits_type::now())
  {
    auto ignored_ec = boost::system::error_code{};
    m_socket.close(ignored_ec);
    m_connected = false;
    m_timer.expires_at(boost::posix_time::pos_infin);
    throw timeout_error{};
  }

  m_timer.async_wait([this](const boost::system::error_code&)
                     {
                       check_timer();
                     });
}

/*------------------------------------------------------------------------------------------------*/

void
LMS1xx::read()
{
  m_buffer.consume(m_buffer.size()); // reset buffer

  m_timer.expires_from_now(m_timeout);
  auto ec = boost::system::error_code{boost::asio::error::would_block};
  boost::asio::async_read_until( m_socket, m_buffer, frame_end
                               , [&](const boost::system::error_code& e, std::size_t)
                                 {
                                   ec = e;
                                 });

  do
  {
    m_io.run_one();
  }
  while (ec == boost::asio::error::would_block);

  if (*std::istreambuf_iterator<char>{&m_buffer} != frame_start)
  {
    throw invalid_telegram_error{};
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

scan_data
LMS1xx::getData()
{
  read();

  /// @todo Directly read from the streambuf
  const auto sz = m_buffer.size();
  auto buf = std::unique_ptr<char[]>(new char[sz]);
  std::copy(std::istreambuf_iterator<char>{&m_buffer}, std::istreambuf_iterator<char>{}, buf.get());
  buf[sz - 1] = '\0';

  /// @todo Rewrite parsing
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

  auto data = scan_data{};

  for (auto i = 0; i < NumberChannels16Bit; ++i)
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

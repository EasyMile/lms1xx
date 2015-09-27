#pragma once

#include <cstdint>
#include <string>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace lms1xx {

/*------------------------------------------------------------------------------------------------*/

/// @brief Structure containing scan configuration
struct scan_configuration
{
	/// @brief Scanning frequency
  ///
  /// From 1 to 100 Hz
	int scaningFrequency;

	/// @brief Scanning resolution
  ///
  /// 1/10000 degree
	int angleResolution;

	/// @brief Start angle
  ///
  /// 1/10000 degree
	int startAngle;

	/// @brief Stop angle
  /// 1/10000 degree
	int stopAngle;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Structure containing scan data configuration
struct scan_data_configuration
{
	/// @brief Output channels
  ///
  /// Defines which output channel is activated.
	int outputChannel;

	//// @brief Remission
  ///
  /// Defines whether remission values are output.
	bool remission;

  /// @brief Remission resolution
  ///
  /// Defines whether the remission values are output with 8-bit or 16bit resolution.
	int resolution;

  /// @brief Encoders channels
  ///
  /// Defines which output channel is activated.
	int encoder;

  /// @brief Position
  ///
  /// Defines whether position values are output.
	bool position;

  /// @brief Device name
  /// Determines whether the device name is to be output
	bool deviceName;

	bool timestamp;

  /// Output interval
  /// Defines which scan is output (1 for 1st, 2 for 2nd, up to 50000).
	int outputInterval;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Structure containing single scan message.
struct scan_data
{

	/// @brief Number of samples in dist1.
	int dist_len1;

	/// @brief Radial distance for the first reflected pulse
	uint16_t dist1[1082];

	/// @brief Number of samples in dist2.
	int dist_len2;

	/// @brief Radial distance for the second reflected pulse
	uint16_t dist2[1082];

	/// @brief Number of samples in rssi1.
	int rssi_len1;

	/// @brief Remission values for the first reflected pulse
	uint16_t rssi1[1082];

	/// @brief Number of samples in rssi2.
	int rssi_len2;

	/// @brief Remission values for the second reflected pulse
	uint16_t rssi2[1082];
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Structure containing scan output range configuration
struct scan_output_range
{
	/// @brief Scanning resolution in 1/10000 degree
	int angleResolution;

	/// @brief Start angle in 1/10000 degree
	int startAngle;

	/// @brief Stop angle in 1/10000 degree
	int stopAngle;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Describe LMS1xx possible statuses
enum class device_status
{
  undefined = 0
, initialisation = 1
, configuration = 2
, idle = 3
, rotated = 4
, in_preparation = 5
, ready = 6
, ready_for_measurement = 7
};

/*------------------------------------------------------------------------------------------------*/

class invalid_telegram_error final
  : public std::exception
{};

/*------------------------------------------------------------------------------------------------*/

class timeout_error final
  : public std::exception
{};

/*------------------------------------------------------------------------------------------------*/

/// @brief Class responsible for communicating with LMS1xx device.
class LMS1xx final
{
public:

  /// @brief Can't copy-construct a LMS1xx
  LMS1xx(const LMS1xx&) = delete;

  /// @brief Can't copy a LMS1xx
  LMS1xx& operator=(const LMS1xx&) = delete;

  /// @brief Can move-construct a LMS1xx
  LMS1xx(LMS1xx&&) = default;

  /// @brief Can move a LMS1xx
  LMS1xx& operator=(LMS1xx&&) = default;

  /// @brief Default constructor
  LMS1xx(const boost::posix_time::time_duration& timeout = boost::posix_time::seconds{30});

  /// @brief Construct that connects to a given device
  LMS1xx( const std::string& host, const std::string& port
        , const boost::posix_time::time_duration& timeout = boost::posix_time::seconds{30});

  /// @brief Destructor.
  ///
  /// Disconnect from device.
  ~LMS1xx();

	/// @brief Connect to LMS1xx
	/// @param host LMS1xx host name or ip address
	/// @param port LMS1xx port number
  /// @note Does nothing if the device is already connected
	void
  connect(const std::string& host, const std::string& port);

	/// @brief Disconnect from LMS1xx device
	void
  disconnect();

	/// @brief Get status of connection
	/// @returns connected or not.
	bool
  connected()
  const noexcept;

	/// @brief Start measurements
  ///
	/// After receiving this command LMS1xx unit starts spinning laser and measuring.
	void
  start_measurements();

	/// @brief Stop measurements
  ///
  /// After receiving this command LMS1xx unit stop spinning laser and measuring.
	void stop_measurements();

	/// @brief Get current status of LMS1xx device
	device_status
  status();

	/// @brief Log into LMS1xx unit.
  ///
	/// Increase privilege level, giving ability to change device configuration.
	void login();

	/// @brief Get current scan configuration
  ///
	/// Get scan configuration :
	/// - scanning frequency.
	/// - scanning resolution.
	/// - start angle.
	/// - stop angle.
	scan_configuration
  get_configuration();

	/// @brief Set scan configuration
  ///
	/// Get scan configuration :
	/// - scanning frequency.
	/// - scanning resolution.
	/// - start angle.
	/// - stop angle.
	void
  set_scan_configuration(const scan_configuration &cfg);

	/// @brief Set scan data configuration
  ///
  /// Set format of scan message returned by device.
	void
  set_scan_data_configuration(const scan_data_configuration &cfg);

  /// @brief Get current output range configuration
  scan_output_range
  get_scan_output_range();

	/// @brief Start or stop continuous data acquisition
  ///
	/// After reception of this command device start or stop continuous data stream containing scan
  /// messages.
	void
  scan_continous(bool start);

	/// @brief Receive single scan message
  scan_data
  getData();

	/// @brief Save data permanently
	/// Parameters are saved in the EEPROM of the LMS and will also be available after the device is
  /// switched off and on again.
	void
  save_configuration();

	/// @brief The device is returned to the measurement mode after configuration
	void
  start_device();

private:

  /// @brief Read a telegram from the device
  /// @note m_buffer is reset before each read
  /// Result will be available in m_buffer.
  void
  read();

  void
  write(const std::string& telegram);

  void
  write(const char* telegram, std::size_t len);

  void
  check_timer();

private:

  /// @brief Manage I/O
  boost::asio::io_service m_io;

  /// @brief The connection to the device
  boost::asio::ip::tcp::socket m_socket;

  /// @brief The buffer of received telegrams
  boost::asio::streambuf m_buffer;

  /// @brief Track timeouts
  boost::asio::deadline_timer m_timer;

  /// @brief True if the device is connected
  bool m_connected;

  /// @brief Time to wait before throwing a timeout exception
  boost::posix_time::time_duration m_timeout;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace lms1xx

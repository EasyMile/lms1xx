/*
 * LMS1xx.h
 *
 *  Created on: 09-08-2010
 *  Author: Konrad Banachowicz
 ***************************************************************************
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Lesser General Public            *
 *   License as published by the Free Software Foundation; either          *
 *   version 2.1 of the License, or (at your option) any later version.    *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 59 Temple Place,                                    *
 *   Suite 330, Boston, MA  02111-1307  USA                                *
 *                                                                         *
 ***************************************************************************/

#pragma once

#include <cstdint>
#include <string>

#include <boost/optional.hpp>

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
  /// Defines which scan is output (first, second, ..., 50000th).
	int outputInterval;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Structure containing single scan message.
struct scanData
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

/// @brief Class responsible for communicating with LMS1xx device.
class LMS1xx final
{
public:

  /// @brief Default constructor
  LMS1xx();

  /// @brief Construct that connects to a given device
  LMS1xx(const std::string& host, unsigned short port);

  /// @brief Destructor.
  ///
  /// Disconnect from device.
  ~LMS1xx();

	/// @brief Connect to LMS1xx
	/// @param host LMS1xx host name or ip address
	/// @param port LMS1xx port number
	void
  connect(const std::string& host, unsigned short port);

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
  get_configuration()
  const;

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

	/// @brief Start or stop continuous data acquisition
  ///
	/// After reception of this command device start or stop continuous data stream containing scan
  /// messages.
	void
  scan_continous(bool start);

	/// @brief Receive single scan message
	boost::optional<scanData>
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

  bool m_connected;

	int sockDesc;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace lms1xx

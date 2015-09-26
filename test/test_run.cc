#include <chrono>
#include <iostream>
#include <thread>

#include "lms1xx/lms1xx.hh"

int
main()
{
  auto cc = lms1xx::scan_data_configuration{};
  cc.outputChannel = 1;
  cc.remission = true;
  cc.resolution = 1;
  cc.encoder = 0;
  cc.position = false;
  cc.deviceName = false;
  cc.outputInterval = 1;

  try
  {
    lms1xx::LMS1xx laser{"192.168.0.1", "2111", boost::posix_time::seconds{30}};
    laser.login();

    laser.set_scan_data_configuration(cc);

    laser.start_measurements();

    while (laser.status() != lms1xx::device_status::ready_for_measurement)
    {
      std::cout << "Waiting for device\n";
      std::this_thread::sleep_for(std::chrono::seconds{1});
    }

    laser.start_device();
    laser.scan_continous(true);

    for (auto i = 0ul; i < 999; ++i)
    {
      try
      {
        if (const auto data_opt = laser.getData())
        {
          const auto& data = *data_opt;
          // std::cout << data.dist_len1 << " " << data.rssi_len1 <<  '\n';

          auto range = 0.0;
          for (int i = 0; i < data.dist_len1; i++)
          {
            // scan_msg.ranges[i] = data.dist1[i] * 0.001;
            // std::cout << data.dist1[i] * 0.001 << '\n';
            range += data.dist1[i] * 0.001;
          }

          auto inten = uint16_t{};
          for (int i = 0; i < data.rssi_len1; i++)
          {
            // scan_msg.intensities[i] = data.rssi1[i];
            // std::cout << data.rssi1[i] << '\n';
            inten += data.rssi1[i];
          }
          std::cout << range << "  " << inten << std::endl;
        }
      }
      catch (const lms1xx::invalid_telegram_error&)
      {
        std::cerr << "Invalid telegram, retrying.";
      }
    }

    laser.login();
    laser.stop_measurements();
    laser.scan_continous(false);
  }
  catch (const lms1xx::timeout_error&)
  {
    std::cout << "Connection lost after 30s\n";
  }
  catch (const std::exception& e)
  {
    std::cerr << "An error happened: " << e.what() << '\n';
    return 1;
  }

	return 0;
}

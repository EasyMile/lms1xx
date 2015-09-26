/*
 * test.cpp
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

#include <chrono>
#include <iostream>
#include <thread>

#include "lms1xx/lms1xx.hh"


int main()
{
  using namespace lms1xx;

  auto laser = LMS1xx{};

	laser.connect("192.168.0.1", 2111);

  if(not laser.isConnected())
	{
		return 0;
	}

	laser.login();

	laser.stopMeas();

	auto conf = laser.getScanCfg();

	conf.angleResolution = 5000;
	conf.scaningFrequency = 5000;

	laser.setScanCfg(conf);

	scanDataCfg cc;
	cc.deviceName = false;
	cc.encoder = 0;
	cc.outputChannel = 3;
	cc.remission = true;
	cc.resolution = 0;
	cc.position = false;
	cc.outputInterval = 1;

	laser.setScanDataCfg(cc);

	laser.startMeas();

  while (laser.queryStatus() != status::ready_for_measurement)
	{
    std::this_thread::sleep_for(std::chrono::seconds{1});
	}

	laser.scanContinous(true);

	for(auto i = 0; i < 3; ++i)
	{
		const auto data = laser.getData();
	}

	laser.scanContinous(false);
	laser.stopMeas();
	laser.disconnect();

	return 0;
}

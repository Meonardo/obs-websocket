/*
obs-websocket
Copyright (C) 2016-2021 Stephane Lepin <stephane.lepin@gmail.com>
Copyright (C) 2020-2021 Kyle Manning <tt2468@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "RequestHandler.h"

RequestResult RequestHandler::GetRecordStatus(const Request&)
{
	OBSOutputAutoRelease recordOutput = obs_frontend_get_recording_output();

	uint64_t outputDuration = Utils::Obs::NumberHelper::GetOutputDuration(recordOutput);

	json responseData;
	responseData["outputActive"] = obs_output_active(recordOutput);
	responseData["outputPaused"] = obs_output_paused(recordOutput);
	responseData["outputTimecode"] = Utils::Obs::StringHelper::DurationToTimecode(outputDuration);
	responseData["outputDuration"] = outputDuration;
	responseData["outputBytes"] = (uint64_t)obs_output_get_total_bytes(recordOutput);

	return RequestResult::Success(responseData);
}

RequestResult RequestHandler::ToggleRecord(const Request&)
{
	json responseData;
	if (obs_frontend_recording_active()) {
		obs_frontend_recording_stop();
		responseData["outputActive"] = false;
	} else {
		obs_frontend_recording_start();
		responseData["outputActive"] = true;
	}

	return RequestResult::Success(responseData);
}

RequestResult RequestHandler::StartRecord(const Request&)
{
	if (obs_frontend_recording_active())
		return RequestResult::Error(RequestStatus::OutputRunning);

	// TODO: Call signal directly to perform blocking wait
	obs_frontend_recording_start();

	return RequestResult::Success();
}

RequestResult RequestHandler::StopRecord(const Request&)
{
	if (!obs_frontend_recording_active())
		return RequestResult::Error(RequestStatus::OutputNotRunning);

	// TODO: Call signal directly to perform blocking wait
	obs_frontend_recording_stop();

	return RequestResult::Success();
}

RequestResult RequestHandler::ToggleRecordPause(const Request&)
{
	json responseData;
	if (obs_frontend_recording_paused()) {
		obs_frontend_recording_pause(false);
		responseData["outputPaused"] = false;
	} else {
		obs_frontend_recording_pause(true);
		responseData["outputPaused"] = true;
	}

	return RequestResult::Success(responseData);
}

RequestResult RequestHandler::PauseRecord(const Request&)
{
	if (obs_frontend_recording_paused())
		return RequestResult::Error(RequestStatus::OutputPaused);

	// TODO: Call signal directly to perform blocking wait
	obs_frontend_recording_pause(true);

	return RequestResult::Success();
}

RequestResult RequestHandler::ResumeRecord(const Request&)
{
	if (!obs_frontend_recording_paused())
		return RequestResult::Error(RequestStatus::OutputNotPaused);

	// TODO: Call signal directly to perform blocking wait
	obs_frontend_recording_pause(false);

	return RequestResult::Success();
}

/**
 * Get the path of  the current recording folder.
 *
 * @return {String} `rec-folder` Path of the recording folder.
 *
 * @api requests
 * @name GetRecordingFolder
 * @category recording
 * @since 4.1.0
 */
RequestResult RequestHandler::GetRecordDirectory(const Request&)
{
	json responseData;
	responseData["recordDirectory"] = Utils::Obs::StringHelper::GetCurrentRecordOutputPath();

	return RequestResult::Success(responseData);
}

/**
 * In the current profile, sets the recording folder of the Simple and Advanced
 * output modes to the specified value.
 *
 * Note: If `SetRecordingDirectory` is called while a recording is
 * in progress, the change won't be applied immediately and will be
 * effective on the next recording.
 *
 * @param {String} `rec-folder` Path of the recording folder.
 *
 * @api requests
 * @name SetRecordingFolder
 * @category recording
 * @since 4.1.0
 */
RequestResult RequestHandler::SetRecordDirectory(const Request& request)
{
	if (!request.Contains("rec-folder")) {
		return RequestResult::Error(RequestStatus::InvalidRequestField, "rec-folder parameter missing");
	}

	std::string newRecFolder = request.RequestData["rec-folder"];
	bool success = Utils::Obs::SetCurrentRecordingFolder(newRecFolder.c_str());
	if (!success) {
		return RequestResult::Error(RequestStatus::InvalidRequestField, "invalid request parameters");
	}

	return RequestResult::Success();
}

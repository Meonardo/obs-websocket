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

#include <algorithm>

#include "Obs.h"
#include "../plugin-macros.generated.h"

static std::vector<std::string> ConvertStringArray(char **array)
{
	std::vector<std::string> ret;
	if (!array)
		return ret;

	size_t index = 0;
	char* value = nullptr;
	do {
		value = array[index];
		if (value)
			ret.push_back(value);
		index++;
	} while (value);

	return ret;
}

std::vector<std::string> Utils::Obs::ArrayHelper::GetSceneCollectionList()
{
	char** sceneCollections = obs_frontend_get_scene_collections();
	auto ret = ConvertStringArray(sceneCollections);
	bfree(sceneCollections);
	return ret;
}

std::vector<std::string> Utils::Obs::ArrayHelper::GetProfileList()
{
	char** profiles = obs_frontend_get_profiles();
	auto ret = ConvertStringArray(profiles);
	bfree(profiles);
	return ret;
}

std::vector<obs_hotkey_t *> Utils::Obs::ArrayHelper::GetHotkeyList()
{
	std::vector<obs_hotkey_t *> ret;

	obs_enum_hotkeys([](void* data, obs_hotkey_id, obs_hotkey_t* hotkey) {
		auto ret = reinterpret_cast<std::vector<obs_hotkey_t *> *>(data);

		ret->push_back(hotkey);

		return true;
	}, &ret);

	return ret;
}

std::vector<std::string> Utils::Obs::ArrayHelper::GetHotkeyNameList()
{
	auto hotkeys = GetHotkeyList();

	std::vector<std::string> ret;
	for (auto hotkey : hotkeys)
		ret.emplace_back(obs_hotkey_get_name(hotkey));

	return ret;
}

std::vector<json> Utils::Obs::ArrayHelper::GetSceneList()
{
	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);

	std::vector<json> ret;
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t *scene = sceneList.sources.array[i];

		if (obs_source_is_group(scene))
			continue;

		json sceneJson;
		sceneJson["sceneName"] = obs_source_get_name(scene);
		sceneJson["sceneIndex"] = sceneList.sources.num - i - 1;

		ret.push_back(sceneJson);
	}

	obs_frontend_source_list_free(&sceneList);

	// Reverse the vector order to match other array returns
	std::reverse(ret.begin(), ret.end());

	return ret;
}

std::vector<json> Utils::Obs::ArrayHelper::GetSceneItemList(obs_scene_t *scene, bool basic)
{
	std::pair<std::vector<json>, bool> enumData;
	enumData.second = basic;

	obs_scene_enum_items(scene, [](obs_scene_t*, obs_sceneitem_t* sceneItem, void* param) {
		auto enumData = reinterpret_cast<std::pair<std::vector<json>, bool>*>(param);

		json item;
		item["sceneItemId"] = obs_sceneitem_get_id(sceneItem);
		// Should be slightly faster than calling obs_sceneitem_get_order_position()
		item["sceneItemIndex"] = enumData->first.size();
		if (!enumData->second) {
			OBSSource itemSource = obs_sceneitem_get_source(sceneItem);
			item["sourceName"] = obs_source_get_name(itemSource);
			item["sourceType"] = StringHelper::GetSourceType(itemSource);
			if (obs_source_get_type(itemSource) == OBS_SOURCE_TYPE_INPUT)
				item["inputKind"] = obs_source_get_id(itemSource);
			else
				item["inputKind"] = nullptr;
			if (obs_source_get_type(itemSource) == OBS_SOURCE_TYPE_SCENE)
				item["isGroup"] = obs_source_is_group(itemSource);
			else
				item["isGroup"] = nullptr;
		}

		enumData->first.push_back(item);

		return true;
	}, &enumData);

	return enumData.first;
}

std::vector<json> Utils::Obs::ArrayHelper::GetTransitionList()
{
	obs_frontend_source_list transitionList = {};
	obs_frontend_get_transitions(&transitionList);

	std::vector<json> ret;
	for (size_t i = 0; i < transitionList.sources.num; i++) {
		obs_source_t *transition = transitionList.sources.array[i];
		json transitionJson;
		transitionJson["transitionName"] = obs_source_get_name(transition);
		transitionJson["transitionKind"] = obs_source_get_id(transition);
		transitionJson["transitionFixed"] = obs_transition_fixed(transition);
		ret.push_back(transitionJson);
	}

	obs_frontend_source_list_free(&transitionList);

	return ret;
}

struct EnumInputInfo {
	std::string inputKind; // For searching by input kind
	std::vector<json> inputs;
};

std::vector<json> Utils::Obs::ArrayHelper::GetInputList(std::string inputKind)
{
	EnumInputInfo inputInfo;
	inputInfo.inputKind = inputKind;

	auto inputEnumProc = [](void *param, obs_source_t *input) {
		// Sanity check in case the API changes
		if (obs_source_get_type(input) != OBS_SOURCE_TYPE_INPUT)
			return true;

		auto inputInfo = reinterpret_cast<EnumInputInfo*>(param);

		std::string inputKind = obs_source_get_id(input);

		if (!inputInfo->inputKind.empty() && inputInfo->inputKind != inputKind)
			return true;

		json inputJson;
		inputJson["inputName"] = obs_source_get_name(input);
		inputJson["inputKind"] = inputKind;
		inputJson["unversionedInputKind"] = obs_source_get_unversioned_id(input);

		inputInfo->inputs.push_back(inputJson);
		return true;
	};
	// Actually enumerates only public inputs, despite the name
	obs_enum_sources(inputEnumProc, &inputInfo);

	return inputInfo.inputs;
}

std::vector<std::string> Utils::Obs::ArrayHelper::GetInputKindList(bool unversioned, bool includeDisabled)
{
	std::vector<std::string> ret;

	size_t idx = 0;
	const char *kind;
	const char *unversioned_kind;
	while (obs_enum_input_types2(idx++, &kind, &unversioned_kind)) {
		uint32_t caps = obs_get_source_output_flags(kind);

		if (!includeDisabled && (caps & OBS_SOURCE_CAP_DISABLED) != 0)
			continue;

		if (unversioned)
			ret.push_back(unversioned_kind);
		else
			ret.push_back(kind);
	}

	return ret;
}

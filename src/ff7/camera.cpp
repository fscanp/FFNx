/****************************************************************************/
//    Copyright (C) 2009 Aali132                                            //
//    Copyright (C) 2018 quantumpencil                                      //
//    Copyright (C) 2018 Maxime Bacoux                                      //
//    Copyright (C) 2020 myst6re                                            //
//    Copyright (C) 2020 Chris Rizzitello                                   //
//    Copyright (C) 2020 John Pritchard                                     //
//    Copyright (C) 2021 Julian Xhokaxhiu                                   //
//    Copyright (C) 2021 Tang-Tang Zhou                                     //
//                                                                          //
//    This file is part of FFNx                                             //
//                                                                          //
//    FFNx is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU General Public License as published by  //
//    the Free Software Foundation, either version 3 of the License         //
//                                                                          //
//    FFNx is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//    GNU General Public License for more details.                          //
/****************************************************************************/

#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <span>

#include "../ff7.h"
#include "../log.h"
#include "../globals.h"

const std::unordered_map<byte, int> numArgsPositionOpCode{{0xD5, 2}, {0xD6, 0}, {0xD7, 2}, {0xD8, 9}, {0xD9, 0}, {0xDA, 0}, {0xDB, 0}, {0xDC, 0}, {0xDD, 1}, {0xDE, 1}, {0xDF, 0}, {0xE0, 2}, {0xE1, 0}, {0xE2, 1}, {0xE3, 9}, {0xE4, 8}, {0xE5, 8}, {0xE6, 7}, {0xE7, 8}, {0xE9, 8}, {0xEB, 9}, {0xEF, 8}, {0xF0, 7}, {0xF1, 0}, {0xF2, 5}, {0xF3, 5}, {0xF4, -1}, {0xF5, 1}, {0xF7, 7}, {0xF8, 12}, {0xF9, 6}, {0xFE, 0}, {0xFF, -1}};
const std::unordered_map<byte, int> numArgsOpCode{{0xD8, 9}, {0xD9, 0}, {0xDB, 0}, {0xDC, 0}, {0xDD, 1}, {0xDE, 1}, {0xDF, 0}, {0xE0, 2}, {0xE1, 0}, {0xE2, 1}, {0xE3, 9}, {0xE4, 8}, {0xE5, 8}, {0xE6, 7}, {0xE8, 8}, {0xEA, 8}, {0xEC, 9}, {0xF0, 8}, {0xF4, -1}, {0xF5, 1}, {0xF8, 7}, {0xF9, 7}, {0xFA, 6}, {0xFE, 0}, {0xFF, -1}};
const std::unordered_set<byte> endingFocalOpCodes{0xF0, 0xF8, 0xF9, 0xFF};
const std::unordered_set<byte> endingPositionOpCodes{0xEF, 0xF0, 0xF7, 0xFF};
constexpr int CAMERA_ARRAY_SIZE = 16;
std::array<bool, CAMERA_ARRAY_SIZE> isNewCameraFunction{};

void patchOpCodeF5(bcamera_position *camera_position, char variationIndex)
{
    camera_position[variationIndex].frames_to_wait = (camera_position[variationIndex].frames_to_wait + 1) * frame_multiplier;
    camera_position[variationIndex].frames_to_wait--; // After 0xF5, there should "always" be an opcode 0xF4
}

byte *getCameraScriptPointer(char variationIndex, short cameraScriptIdx, bool isSub5C3FD5)
{
    int internalOffset = isSub5C3FD5 ? 4 : 0;
    if (cameraScriptIdx == -1)
        return isSub5C3FD5 ? ff7_externals.battle_camera_focal_scripts_8FEE30 : ff7_externals.battle_camera_position_scripts_8FEE2C;
    else if (cameraScriptIdx == -2)
        return isSub5C3FD5 ? (byte *)ff7_externals.battle_camera_focal_scripts_901270[*ff7_externals.battle_camera_script_index] : (byte *)ff7_externals.battle_camera_position_scripts_9010D0[*ff7_externals.battle_camera_script_index];
    else if (cameraScriptIdx == -3)
    {
        int outerOffset = variationIndex * 4 + *(int *)(*ff7_externals.battle_camera_global_scripts_9A13BC + 0x8 + internalOffset) - *ff7_externals.battle_camera_script_offset;
        int finalOffset = *(int *)(*ff7_externals.battle_camera_global_scripts_9A13BC + outerOffset) - *ff7_externals.battle_camera_script_offset;
        return (byte *)(*ff7_externals.battle_camera_global_scripts_9A13BC + finalOffset);
    }
    int outerOffset = (3 * cameraScriptIdx + variationIndex) * 4 + *(int *)(*ff7_externals.battle_camera_global_scripts_9A13BC + internalOffset) - *ff7_externals.battle_camera_script_offset;
    int finalOffset = *(int *)(*ff7_externals.battle_camera_global_scripts_9A13BC + outerOffset) - *ff7_externals.battle_camera_script_offset;
    return (byte *)(*ff7_externals.battle_camera_global_scripts_9A13BC + finalOffset);
}

bool simulateCameraScript(byte *scriptPtr, uint16_t &currentPosition, uint16_t &framesToWait, const std::unordered_map<byte, int> &numArgsOpCode,
                          const std::unordered_set<byte> &endingOpCodes)
{
    if (trace_all || trace_battle_camera)
        ffnx_trace("%s - START LIST OF CAMERA SCRIPT OPCODE AND ARGS\n", __func__);

    bool executedOpCodeF5 = false;
    bool isScriptActive = true;
    while (isScriptActive)
    {
        byte currentOpCode = scriptPtr[currentPosition++];

        if (trace_all || trace_battle_camera)
            ffnx_trace("opcode: 0x%0x\n", currentOpCode);

        switch (currentOpCode)
        {
        case 0xF4:
            if (framesToWait != 0)
            {
                framesToWait--;
                currentPosition--;
                isScriptActive = false;
            }
            break;
        case 0xF5:
            executedOpCodeF5 = true;
            framesToWait = scriptPtr[currentPosition++] * frame_multiplier;
            break;
        case 0xFE:
            if (framesToWait == 0)
            {
                currentOpCode = scriptPtr[currentPosition];

                if (trace_all || trace_battle_camera)
                    ffnx_trace("0xFE case: opcode 0x%0x\n", currentOpCode);

                if (currentOpCode == 192)
                {
                    framesToWait = 0;
                    currentPosition = 0;
                }
            }
            break;
        default:
            if (numArgsOpCode.contains(currentOpCode))
            {
                currentPosition += numArgsOpCode.at(currentOpCode);

                if (endingOpCodes.contains(currentOpCode))
                    isScriptActive = false;
            }
            else
            {
                if (trace_all || trace_battle_camera)
                    ffnx_error("%s - Strange OpCode 0x%0x in camera script\n", __func__, currentOpCode);
                isScriptActive = false;
            }
            break;
        }
    }
    if (trace_all || trace_battle_camera)
        ffnx_trace("%s - END LIST OF CAMERA SCRIPT OPCODE AND ARGS\n", __func__);

    return executedOpCodeF5;
}

int ff7_add_fn_to_camera_fn(uint32_t function)
{
    auto cameraArray = std::span<uint32_t>(ff7_externals.camera_fn_array, CAMERA_ARRAY_SIZE);
    auto element = std::find(cameraArray.begin(), cameraArray.end(), 0);
    if (element != cameraArray.end())
    {
        int index = std::distance(cameraArray.begin(), element);
        ff7_externals.camera_fn_array[index] = function;
        ff7_externals.camera_fn_data[index].field_0 = *ff7_externals.camera_fn_index;
        *ff7_externals.camera_fn_counter = *ff7_externals.camera_fn_counter + 1;

        isNewCameraFunction[index] = true;
        return index;
    }
    return 0xFFFF;
}

void ff7_execute_camera_functions()
{
    uint16_t &fn_index = *ff7_externals.camera_fn_index;
    for (fn_index = 0; fn_index < CAMERA_ARRAY_SIZE; fn_index++)
    {
        if (ff7_externals.camera_fn_array[fn_index] != 0)
        {
            if (isNewCameraFunction[fn_index])
            {
                if (ff7_externals.camera_fn_array[fn_index] == ff7_externals.battle_camera_position_sub_5C5B9C ||
                    ff7_externals.camera_fn_array[fn_index] == ff7_externals.battle_camera_focal_sub_5C5F5E ||
                    ff7_externals.camera_fn_array[fn_index] == ff7_externals.battle_camera_position_sub_5C557D ||
                    ff7_externals.camera_fn_array[fn_index] == ff7_externals.battle_camera_focal_sub_5C5714)
                {
                    ff7_externals.camera_fn_data[fn_index].n_frames *= frame_multiplier;
                }
                else if (ff7_externals.camera_fn_array[fn_index] == ff7_externals.battle_camera_position_sub_5C3D0D)
                {
                    ff7_externals.camera_fn_data[fn_index].n_frames *= frame_multiplier;
                    ff7_externals.camera_fn_data[fn_index].field_8 /= frame_multiplier;
                    ff7_externals.camera_fn_data[fn_index].field_6 /= frame_multiplier;
                    ff7_externals.camera_fn_data[fn_index].field_E /= frame_multiplier;
                }

                if (trace_all || trace_battle_camera)
                    ffnx_trace("%s - executing function: 0x%x\n", __func__, ff7_externals.camera_fn_array[fn_index]);

                isNewCameraFunction[fn_index] = false;
            }

            ((void (*)())ff7_externals.camera_fn_array[fn_index])();
            if (ff7_externals.camera_fn_data[fn_index].field_0 == (uint16_t)-1)
            {
                ff7_externals.camera_fn_data[fn_index].field_0 = 0;
                ff7_externals.camera_fn_data[fn_index].field_2 = 0;
                ff7_externals.camera_fn_array[fn_index] = 0;
                *ff7_externals.camera_fn_counter = *ff7_externals.camera_fn_counter - 1;
            }
        }
    }
    fn_index = 0;
}

void ff7_run_camera_focal_position_script(char variationIndex, DWORD param_2, short cameraScriptIdx)
{
    if (trace_all || trace_battle_camera)
        ffnx_trace("%s - Parameters: %d, %d, %d\n", __func__, variationIndex, param_2, cameraScriptIdx);

    bcamera_position *cameraPosition = ff7_externals.battle_camera_focal_position;

    byte *scriptPtr = getCameraScriptPointer(variationIndex, cameraScriptIdx, true);
    uint16_t currentPosition = (cameraPosition[variationIndex].current_position == 255) ? 0 : cameraPosition[variationIndex].current_position;
    uint16_t framesToWait = (cameraPosition[variationIndex].current_position == 255) ? 0 : cameraPosition[variationIndex].frames_to_wait;

    bool executedOpCodeF5 = simulateCameraScript(scriptPtr, currentPosition, framesToWait, numArgsOpCode, endingFocalOpCodes);

    ((void (*)(char, DWORD, short))ff7_externals.set_camera_focal_position_scripts)(variationIndex, param_2, cameraScriptIdx);

    if (executedOpCodeF5)
        patchOpCodeF5(cameraPosition, variationIndex);

    if (currentPosition != cameraPosition[variationIndex].current_position)
        ffnx_error("%s - Camera script pointer simulation wrong! Battle camera final position does not match (simulation: %d != real: %d)\n", __func__,
                   currentPosition, cameraPosition[variationIndex].current_position);

    if (framesToWait != cameraPosition[variationIndex].frames_to_wait)
        ffnx_error("%s - Camera script pointer simulation wrong! Battle camera final frames to wait does not match (simulation: %d != real: %d)\n", __func__,
                   framesToWait, cameraPosition[variationIndex].frames_to_wait);
}

void ff7_run_camera_position_script(char variationIndex, DWORD param_2, short cameraScriptIdx)
{
    if (trace_all || trace_battle_camera)
        ffnx_trace("%s - Parameters: %d, %d, %d\n", __func__, variationIndex, param_2, cameraScriptIdx);

    bcamera_position *cameraPosition = ff7_externals.battle_camera_position;

    byte *scriptPtr = getCameraScriptPointer(variationIndex, cameraScriptIdx, false);
    uint16_t currentPosition = (cameraPosition[variationIndex].current_position == 255) ? 0 : cameraPosition[variationIndex].current_position;
    uint16_t framesToWait = (cameraPosition[variationIndex].current_position == 255) ? 0 : cameraPosition[variationIndex].frames_to_wait;

    bool executedOpCodeF5 = simulateCameraScript(scriptPtr, currentPosition, framesToWait, numArgsPositionOpCode, endingPositionOpCodes);

    ((void (*)(char, DWORD, short))ff7_externals.set_camera_position_scripts)(variationIndex, param_2, cameraScriptIdx);

    if (executedOpCodeF5)
        patchOpCodeF5(cameraPosition, variationIndex);

    if (currentPosition != cameraPosition[variationIndex].current_position)
        ffnx_error("%s - Camera script pointer simulation wrong! Battle camera final position does not match (simulation: %d != real: %d)\n", __func__,
                   currentPosition, cameraPosition[variationIndex].current_position);

    if (framesToWait != cameraPosition[variationIndex].frames_to_wait)
        ffnx_error("%s - Camera script pointer simulation wrong! Battle camera final frames to wait does not match (simulation: %d != real: %d)\n", __func__,
                   framesToWait, cameraPosition[variationIndex].frames_to_wait);
}

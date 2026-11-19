/*
 * VDXROVR
 *
 * Copyright (C) 2022 Fred Emmott <fred@fredemmott.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

class VDXROVRAPIError extends Error {
  constructor(message, apiMethodName) {
    super(message)
    this.apiMethodName = apiMethodName;
  }

  apiMethodName;
}

var VDXROVRNative = new class {
  async asyncRequest(name, ...args) {
    native function VDXROVRNative_AsyncRequest();
    var json = await VDXROVRNative_AsyncRequest(`vdxrovrjs/${name}`, JSON.stringify(args));
    var response = JSON.parse(json);
    if (response.result) {
      return response.result;
    }
    console.log(`\u26a0 VDXROVR API error: '${name}()' => '${response.error}'`);
    throw new VDXROVRAPIError(response.error, name);
  }

  onEvent(callback) {
    native function VDXROVRNative_AddEventCallback();
    VDXROVRNative_AddEventCallback(callback);
  }

  get initializationData() {
    native function VDXROVRNative_GetInitializationData();
    return JSON.parse(VDXROVRNative_GetInitializationData());
  }
};

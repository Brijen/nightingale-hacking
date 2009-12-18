/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 :miv */
/*
 *=BEGIN SONGBIRD GPL
 *
 * This file is part of the Songbird web player.
 *
 * Copyright(c) 2005-2009 POTI, Inc.
 * http://www.songbirdnest.com
 *
 * This file may be licensed under the terms of of the
 * GNU General Public License Version 2 (the ``GPL'').
 *
 * Software distributed under the License is distributed
 * on an ``AS IS'' basis, WITHOUT WARRANTY OF ANY KIND, either
 * express or implied. See the GPL for the specific language
 * governing rights and limitations.
 *
 * You should have received a copy of the GPL along with this
 * program. If not, go to http://www.gnu.org/licenses/gpl.html
 * or write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *=END SONGBIRD GPL
 */

/**
* \file  DeviceMgmtTabs.js
* \brief Javascript source for the device sync tabs widget.
*/
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// Device sync tabs widget.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
// Device sync tabs defs.
//
//------------------------------------------------------------------------------

// Component manager defs.
if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;

Cu.import("resource://app/jsmodules/sbLibraryUtils.jsm");
Cu.import("resource://app/jsmodules/sbProperties.jsm");
Cu.import("resource://app/jsmodules/StringUtils.jsm");

if (typeof(XUL_NS) == "undefined")
  var XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var DeviceMgmtTabs = {
  //
  // Device Sync Tabs object fields.
  //
  //   _widget                  Device sync tabs widget.
  //   _deviceID                Device ID.
  //   _device                  sbIDevice object.
  //

  _widget: null,
  _deviceID: null,
  _device: null,

  /**
   * \brief Initialize the device sync tabs services for the device sync tabs
   *        widget specified by aWidget.
   *
   * \param aWidget             Device sync tabs widget.
   */

  initialize: function DeviceMgmtTabs_initialize(aWidget) {
    // Get the device widget.
    this._widget = aWidget;

    // Initialize object fields.
    this._deviceID = this._widget.deviceID;
    this._device = this._widget.device;

    // Check what content this device supports then hide tabs for unsupported
    // content types.
    var sbIDC = Ci.sbIDeviceCapabilities;
    // Check Audio
    if (!this._deviceSupportsContent(sbIDC.CONTENT_AUDIO,
                                     sbIDC.FUNCTION_AUDIO_PLAYBACK)) {
      this._toggleTab("music", true);
    }

    // Check Video
    if (!this._deviceSupportsContent(sbIDC.CONTENT_VIDEO,
                                     sbIDC.FUNCTION_VIDEO_PLAYBACK)) {
      this._toggleTab("video", true);
    }
  },

  /**
   * \brief Finalize the device progress services.
   */

  finalize: function DeviceMgmtTabs_finalize() {
    this._device = null;
    this._deviceID = null;
    this._widget = null;
  },

  /**
   * \brief Check if the device has the requested function capabilities.
   *
   * \param aFunctionType            Function type we wish to test for.
   * \sa sbIDeviceCapabilities.idl
   */

  _checkFunctionSupport:
    function DeviceMgmtTabs__checkFunctionSupport(aFunctionType) {
    try {
      var functionTypes = this._device
                              .capabilities
                              .getSupportedFunctionTypes({});
      for (var i in functionTypes) {
        if (functionTypes[i] == aFunctionType) {
          return true;
        }
      }
    }
    catch (err) {
      var strFunctionType;
      switch (aFunctionType) {
        case Ci.sbIDeviceCapabilities.FUNCTION_DEVICE:
          strFunctionType = "Device";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_AUDIO_PLAYBACK:
          strFunctionType = "Audio Playback";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_AUDIO_CAPTURE:
          strFunctionType = "Audio Capture";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_IMAGE_DISPLAY:
          strFunctionType = "Image Display";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_IMAGE_CAPTURE:
          strFunctionType = "Image Capture";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_VIDEO_PLAYBACK:
          strFunctionType = "Video Playback";
          break;
        case Ci.sbIDeviceCapabilities.FUNCTION_VIDEO_CAPTURE:
          strFunctionType = "Video Capture";
          break;
        default:
          strFunctionType = "Unknown";
          break;
      }
      Cu.reportError("Unable to determine if device supports function type: "+
                     strFunctionType + " [" + err + "]");
    }

    return false;
  },

  /**
   * \brief Check if the device has the requested content capabilities in the
   * requested function area.
   *
   * \param aContentType             Content type we wish to test for.
   * \param aFunctionType            Functional area to test in.
   * \sa sbIDeviceCapabilities.idl
   */

  _deviceSupportsContent:
    function DeviceMgmtTabs__deviceSupportsContent(aContentType,
                                                   aFunctionType) {
    // First check if the device supports the function
    if (!this._checkFunctionSupport(aFunctionType)) {
      return false;
    }

    // Now check if the device supports the content type in that function.
    try {
      var contentTypes = this._device
                             .capabilities
                             .getSupportedContentTypes(aFunctionType, {});
      for (var i in contentTypes) {
        if (contentTypes[i] == aContentType) {
          return true;
        }
      }
    }
    catch (err) {
      var strContentType;
      switch (aContentType) {
        case Ci.sbIDeviceCapabilities.CONTENT_AUDIO:
          strContentType = "Audio";
          break;
        case Ci.sbIDeviceCapabilities.CONTENT_VIDEO:
          strConentType = "Video";
          break;
        default:
          strContentType = "Unknown";
          break;
      }
      Cu.reportError("Unable to determine if device supports content: "+
                     strContentType + " [" + err + "]");
    }

    return false;
  },

  /**
   * \brief Hide or show a tab that should not be visible due to the device not
   * supporting them.
   *
   * \param aTabType              reference name of tab to hide.
   *    these are currenly "music","video","tools","settings"
   * \param shouldHide            should we hide this tab.
   */

  _toggleTab: function DeviceMgmtTabs__toggleTab(aTabType, shouldHide) {
    var tab = this._getElement("device_management_" + aTabType + "_sync_tab");

    if (shouldHide) {
      tab.setAttribute("hidden", true);
    }
    else {
      tab.removeAttribute("hidden");
    }
  },

  //----------------------------------------------------------------------------
  //
  // Device sync tabs XUL services.
  //
  //----------------------------------------------------------------------------

  /**
   * \brief Return the XUL element with the ID specified by aElementID.  Use the
   *        element "sbid" attribute as the ID.
   *
   * \param aElementID          ID of element to get.
   *
   * \return Element.
   */

  _getElement: function DeviceMgmtTabs__getElement(aElementID) {
    return document.getAnonymousElementByAttribute(this._widget,
                                                   "sbid",
                                                   aElementID);
  }
};
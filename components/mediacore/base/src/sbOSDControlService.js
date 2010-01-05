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

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

if (typeof(Cc) == "undefined")
  var Cc = Components.classes;
if (typeof(Ci) == "undefined")
  var Ci = Components.interfaces;
if (typeof(Cu) == "undefined")
  var Cu = Components.utils;
if (typeof(Cr) == "undefined")
  var Cr = Components.results;


//------------------------------------------------------------------------------
// Constants

const SB_OSDHIDE_DELAY = 3000;


//==============================================================================
//
// @interface sbOSDControlService
// @brief Service to provide on-screen-display controls for video playback.
//
//==============================================================================

function sbOSDControlService()
{
  this._cloakService = Cc["@songbirdnest.com/Songbird/WindowCloak;1"]
                         .getService(Ci.sbIWindowCloak);
  this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
}

sbOSDControlService.prototype =
{
  _videoWindow:           null,
  _videoWindowFullscreen: false,
  _osdWindow:             null,
  _cloakService:          null,
  _timer:                 null,
  _osdControlsShowing:    false,
  
  _videoWinHasFocus:      false,
  _osdWinHasFocus:        false,
  _mouseDownOnOSD:        false,
  
  _fadeInterval: null,
  _fadeContinuation: null,


  _recalcOSDPosition: function() {
    // This is only necessary when we are not in fullscreen.
    // If we were to do this while in fullscreen, it would re-anchor
    // the controls onto the wrong window.
    if(!this._videoWindowFullscreen) {
      this._osdWindow.moveTo(this._videoWindow.screenX,
                             this._videoWindow.screenY);
      this._osdWindow.resizeTo(this._videoWindow.innerWidth,
                               this._videoWindow.outerHeight);
    }
  },

  //----------------------------------------------------------------------------
  // sbIOSDControlService

  onVideoWindowOpened: function(aVideoWindow) {
    // For now, just open the OSD control pane thingy.
    this._videoWindow = aVideoWindow.QueryInterface(Ci.nsIDOMWindowInternal);

    // Create a OSD overlay window.
    this._osdWindow = this._videoWindow.openDialog(
        "chrome://songbird/content/xul/videoWindowControls.xul",
        "Songbird OSD Control Window",
        "chrome,dependent,modal=no,titlebar=no",
        null);
    this._osdWindow.QueryInterface(Ci.nsIDOMWindowInternal);

    // Cloak the window right now.
    this._cloakService.cloak(this._osdWindow);

    try {
      // Not all platforms have this service.
      var winMoveService =
        Cc["@songbirdnest.com/integration/window-move-resize-service;1"]
        .getService(Ci.sbIWindowMoveService);

      winMoveService.startWatchingWindow(this._videoWindow, this);
    }
    catch (e) {
      // No window move service on this platform.
    }

    // Listen for blur and focus events for determing when both the video
    // window and the OSD controls loose focus.
    var self = this;
    this._osdWinBlurListener = function(aEvent) {
      self._onOSDWinBlur(aEvent);
    };
    this._videoWinBlurListener = function(aEvent) {
      self._onVideoWinBlur(aEvent);
    };
    this._osdWinFocusListener = function(aEvent) {
      self._onOSDWinFocus(aEvent);
    };
    this._videoWinFocusListener = function(aEvent) {
      self._onVideoWinFocus(aEvent);
    };
    this._osdWinMousemoveListener = function(aEvent) {
      self._onOSDWinMousemove(aEvent);
    };
    this._osdWinMousedownListener = function(aEvent) {
      self._onOSDWinMousedown(aEvent);
    };
    this._osdWinMouseupListener = function(aEvent) {
      self._onOSDWinMouseup(aEvent);
    };
    this._osdWindow.addEventListener("blur",
                                     this._osdWinBlurListener,
                                     false);
    this._osdWindow.addEventListener("focus",
                                     this._osdWinFocusListener,
                                     false);
    this._osdWindow.addEventListener("mousemove",
                                     this._osdWinMousemoveListener,
                                     false);
    this._osdWindow.addEventListener("mousedown",
                                     this._osdWinMousedownListener,
                                     false);
    this._osdWindow.addEventListener("mouseup",
                                     this._osdWinMouseupListener,
                                     false);
    this._videoWindow.addEventListener("blur",
                                       this._videoWinBlurListener,
                                       false);
    this._videoWindow.addEventListener("focus",
                                       this._videoWinFocusListener,
                                       false);
  },

  onVideoWindowWillClose: function() {
    try {
      // Not all platforms have this service.
      var winMoveService =
        Cc["@songbirdnest.com/integration/window-move-resize-service;1"]
        .getService(Ci.sbIWindowMoveService);

      winMoveService.stopWatchingWindow(this._videoWindow, this);
    }
    catch (e) {
      // No window move service on this platform.
    }
    
    this._timer.cancel();
    this._osdWindow.removeEventListener("blur",
                                        this._osdWinBlurListener,
                                        false);
    this._osdWindow.removeEventListener("focus",
                                        this._osdWinFocusListener,
                                        false);
    this._osdWindow.removeEventListener("mousemove",
                                        this._osdWinMousemoveListener,
                                        false);
    this._osdWindow.removeEventListener("mousedown",
                                        this._osdWinMousedownListener,
                                        false);
    this._osdWindow.removeEventListener("mouseup",
                                        this._osdWinMouseupListener,
                                        false);
    this._videoWindow.removeEventListener("blur",
                                          this._videoWinBlurListener,
                                          false);
    this._videoWindow.removeEventListener("focus",
                                          this._videoWinFocusListener,
                                          false);
    this._osdWindow.close();
    this._osdWindow = null;
    this._videoWindow = null;
  },

  onVideoWindowResized: function() {
    this._recalcOSDPosition();
  },
  
  onVideoWindowFullscreenChanged: function(aFullscreen) {
    this._videoWindowFullscreen = aFullscreen;
    this._osdWindow.fullScreen = aFullscreen;

    var outterBox = 
      this._osdWindow.document.getElementById("osd_wrapper_hbox");
    
    if(outterBox) {
      if(aFullscreen) {
        outterBox.setAttribute("fullscreen", true);
      }
      else {
        outterBox.removeAttribute("fullscreen");
      }
    }
  },

  hideOSDControls: function(aTransitionType) {
    var self = this;
    // Don't hide the window while the user is dragging
    if (this._mouseDownOnOSD || !this._osdControlsShowing)
      return;

    var transtion;
    switch (aTransitionType) {
      case Ci.sbIOSDControlService.TRANSITION_FADE:
        transition = this._fadeOut;
        break;

      case Ci.sbIOSDControlService.TRANSITION_NONE:
        transition = this._hideInstantly;
        break;

      default:
        Components.utils.reportError(
          "Invalid transition type passed into hideOSDControls()!");
        
        // Just fall back to hiding instantly.
        transition = this._hideInstantly; 
    }

    transition.apply(this, [function() {
      if (!self._cloakService.isCloaked(self._osdWindow)) {
        if (this._osdWinHasFocus) {
          this._videoWindow.focus();
        }
        self._cloakService.cloak(self._osdWindow);
      }

      // The OSD controls are no longer showing
      self._osdControlsShowing = false;
    }]);
  },

  showOSDControls: function(aTransitionType) {
    if (!this._videoWinHasFocus &&
        !this._osdWinHasFocus &&
        !this._videoWindowFullscreen)
    {
      // Don't bother showing the controls if the video and the OSD window have
      // lost focus. This prevents floating the OSD controls ontop of every
      // other window in the OS.
      return;
    }
    
    this._timer.cancel();
    this._recalcOSDPosition();

    // Show the controls if they are currently hidden.
    if (this._cloakService.isCloaked(this._osdWindow)) {
      this._cloakService.uncloak(this._osdWindow);
    }

    var transtion;
    switch (aTransitionType) {
      case Ci.sbIOSDControlService.TRANSITION_FADE:
        transition = this._fade;
        break;

      case Ci.sbIOSDControlService.TRANSITION_NONE:
        transition = this._showInstantly;
        break;

      default:
        Components.utils.reportError(
          "Invalid transition type passed into hideOSDControls()!");
        
        // Just fall back to show instantly.
        transition = this._showInstantly; 
    }
    
    transition.apply(this);

    // Controls are showing
    this._osdControlsShowing = true;

    // Set the timer for hiding.
    this._timer.initWithCallback(this,
                                 SB_OSDHIDE_DELAY,
                                 Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _fade: function(start, end, func) {
    var self = this;
    this._fadeCancel();
    this._fadeContinuation = func;
    var node = this._osdWindow.document.getElementById("osd_main_vbox");
    var opacity = start;
    var delta = (end - start) / 10;
    var step = 1;

    self._fadeInterval = self._osdWindow.setInterval(function() {
      opacity += delta;
      node.style.opacity = opacity;

      if (step++ >= 9) {
        self._fadeCancel();
        node = null;
      }
    }, 50);
  },
  
  _fadeCancel: function() {
    this._osdWindow.clearInterval(this._fadeInterval);
    if (this._fadeContinuation) {
       this._fadeContinuation();
    }
    this._fadeContinuation = null;
  },

  _showInstantly: function(func) {
    this._fadeCancel();
    this._osdWindow.document.getElementById("osd_main_vbox")
                            .style.opacity = 1;
    if (func) {
       func();
    }
  },

  _hideInstantly: function(func) {
    this._fadeCancel();
    if (func) {
       func();
    }
  },

  // fade from 100% opaque to 0% opaque 
  _fadeOut: function(func) {
    this._fade(1, 0, func);
  },

  _onOSDWinBlur: function(aEvent) {
    this._osdWinHasFocus = false;
  },

  _onOSDWinFocus: function(aEvent) {
    this._osdWinHasFocus = true;
  },

  _onVideoWinBlur: function(aEvent) {
    this._videoWinHasFocus = false;
  },

  _onVideoWinFocus: function(aEvent) {
    this._videoWinHasFocus = true;
  },

  _onOSDWinMousemove: function(aEvent) {
    // The user has the mouse over the OSD controls, ensure that the controls
    // are visible.
    this.showOSDControls(Ci.sbIOSDControlService.TRANSITION_NONE);
  },

  _onOSDWinMousedown: function(aEvent) {
    if (aEvent.button == 0) {
      this._mouseDownOnOSD = true;
    }
  },

  _onOSDWinMouseup: function(aEvent) {
    if (aEvent.button == 0) {
      // User released the mouse button, reset the timer
      this._mouseDownOnOSD = false;
      this.showOSDControls(Ci.sbIOSDControlService.TRANSITION_NONE);
    }
  },

  //----------------------------------------------------------------------------
  // sbIWindowMoveListener

  onMoveStarted: function() {
    this.hideOSDControls(Ci.sbIOSDControlService.TRANSITION_NONE);
  },

  onMoveStopped: function() {
    this.showOSDControls(Ci.sbIOSDControlService.TRANSITION_NONE);
  },

  //----------------------------------------------------------------------------
  // nsITimerCallback

  notify: function(aTimer) {
    if (aTimer == this._timer) {
      this.hideOSDControls(Ci.sbIOSDControlService.TRANSITION_FADE);
    }
  },
};

//------------------------------------------------------------------------------
// XPCOM Registration

sbOSDControlService.prototype.classDescription =
  "Songbird OSD Control Service";
sbOSDControlService.prototype.className =
  "sbOSDControlService";
sbOSDControlService.prototype.classID =
  Components.ID("{03F78779-FCB7-4442-9A0C-E8547B4F1368}");
sbOSDControlService.prototype.contractID =
  "@songbirdnest.com/mediacore/osd-control-service;1";
sbOSDControlService.prototype.QueryInterface =
  XPCOMUtils.generateQI([Ci.sbIOSDControlService,
                         Ci.sbIWindowMoveListener,
                         Ci.nsITimerCallback]);

function NSGetModule(compMgr, fileSpec)
{
  return XPCOMUtils.generateModule([sbOSDControlService]);
}


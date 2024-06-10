// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.view.Window;
import android.view.WindowManager;
import android.view.View;

/**
 * A library of utility functions for class #Window.
 */
class WindowUtil {
  static final int FULL_SCREEN_WINDOW_FLAGS =
    WindowManager.LayoutParams.FLAG_FULLSCREEN|

    /* Workaround for layout problems in Android KitKat with immersive full
       screen mode: Sometimes the content view was not initialized with the
       correct size, which caused graphics artifacts. */
    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN|
    WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS|
    WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR|
    WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION;

  /**
   * Set / Reset the System UI visibility flags for Immersive Full
   * Screen Mode.
   */
  static void enableImmersiveMode(Window window) {
  }

  static void disableImmersiveMode(Window window) {
  }

  static void enterFullScreenMode(Window window) {
  }

  static void leaveFullScreenMode(Window window, int preserveFlags) {
  }
}

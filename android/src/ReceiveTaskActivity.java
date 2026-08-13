// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;

public class ReceiveTaskActivity extends Activity {
  private static final String TAG = "XCSoar";

  private static final String SCHEME = "xctsk:";

  /**
   * Hack: this is set by onCreate(), to support the "testing"
   * package.
   */
  protected static Class<?> mainActivityClass;

  @Override protected void onCreate(Bundle savedInstanceState) {
    if (mainActivityClass == null)
      mainActivityClass = XCSoar.class;

    super.onCreate(savedInstanceState);

    final String msg = handleIntent(getIntent());
    if (msg == null) {
      /* the data was handled successfully; don't leave this activity
         behind as an empty window in its own task */
      finish();
      return;
    }

    showError(msg);
  }

  /**
   * Show the error message in a dismissable dialog; finish this
   * activity as soon as the user has acknowledged it, so we never get
   * stuck on a blank screen.
   */
  private void showError(final String msg) {
    Log.w(TAG, "Failed to receive task: " + msg);

    new AlertDialog.Builder(this)
      .setTitle("XCSoar")
      .setMessage(msg)
      .setPositiveButton(android.R.string.ok, null)
      .setOnDismissListener(new DialogInterface.OnDismissListener() {
          @Override public void onDismiss(DialogInterface dialog) {
            finish();
          }
        })
      .show();
  }

  private String handleIntent(final Intent intent) {
    if (intent == null)
      return "No action";

    final String data = intent.getDataString();
    if (data == null)
      return "No action";

    if (!Loader.loaded)
      return Loader.error != null ? Loader.error : "Error";

    Log.d(TAG, "Received intent data='" + data + "'");

    /* the URI scheme is case insensitive; XCTrack QR codes use the
       upper case "XCTSK:" */
    if (!data.regionMatches(true, 0, SCHEME, 0, SCHEME.length()))
      return "Unknown action";

    final String msg =
      NativeView.onReceiveXCTrackTask(data.substring(SCHEME.length()));
    if (msg != null)
      return msg;

    /* the data was handled successfully, and the main "XCSoar"
       activity shows the details - switch to it */
    startActivity(new Intent(this, mainActivityClass));
    return null;
  }
}

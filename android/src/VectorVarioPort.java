// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

package org.xcsoar;

import java.io.IOException;
import java.util.Queue;
import java.util.LinkedList;

import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.os.Build;
import android.util.Log;

/**
 * Combined BLE port for Vector Vario that handles both GPS (via HM-10 FFE1)
 * and Vario data in a single GATT connection.
 *
 * GPS NMEA data goes through the port's InputListener (like HM10Port).
 * Vario data goes through SensorListener.onVarioSensor().
 * 
 * @see https://vectorvario.com/en/developers/
 */
public class VectorVarioPort
    extends BluetoothGattCallback
    implements AndroidPort {
  private static final String TAG = "XCSoar";

  private static final int MAX_WRITE_CHUNK_SIZE = 20;

  /* Maximum number of milliseconds to wait for disconnected state after
     calling BluetoothGatt.disconnect() in close() */
  private static final int DISCONNECT_TIMEOUT = 500;

  private PortListener portListener;
  private volatile InputListener listener;
  private final SensorListener sensorListener;
  private final SafeDestruct safeDestruct = new SafeDestruct();

  private final BluetoothGatt gatt;
  private BluetoothGattCharacteristic dataCharacteristic;
  private BluetoothGattCharacteristic deviceNameCharacteristic;
  private BluetoothGattCharacteristic varioCharacteristic;
  private BluetoothGattCharacteristic windSpeedCharacteristic;
  private BluetoothGattCharacteristic windDirectionCharacteristic;
  private volatile boolean shutdown = false;

  private final HM10WriteBuffer writeBuffer = new HM10WriteBuffer();

  private volatile int portState = STATE_LIMBO;

  private final Object gattStateSync = new Object();
  private int gattState = BluetoothGatt.STATE_DISCONNECTED;

  private boolean setupCharacteristicsPending = false;

  /* Queue-based notification subscription (only one GATT operation at a time) */
  private BluetoothGattCharacteristic currentEnableNotification;
  private final Queue<BluetoothGattCharacteristic> enableNotificationQueue =
    new LinkedList<BluetoothGattCharacteristic>();

  /* Cached wind values (both characteristics report independently) */
  private int lastWindSpeedCmps = 0;
  private int lastWindDirCentideg = 0;

  public VectorVarioPort(Context context, BluetoothDevice device,
                         SensorListener sensorListener)
    throws IOException
  {
    this.sensorListener = sensorListener;

    if (Build.VERSION.SDK_INT >= 23)
      gatt = device.connectGatt(context, true, this, BluetoothDevice.TRANSPORT_LE);
    else
      gatt = device.connectGatt(context, true, this);

    if (gatt == null)
      throw new IOException("Bluetooth GATT connect failed");
  }

  private void findCharacteristics() throws Error {
    dataCharacteristic = null;
    deviceNameCharacteristic = null;
    varioCharacteristic = null;
    windSpeedCharacteristic = null;
    windDirectionCharacteristic = null;

    BluetoothGattService service = gatt.getService(BluetoothUuids.HM10_SERVICE);
    if (service != null) {
      dataCharacteristic = service.getCharacteristic(BluetoothUuids.HM10_RX_TX_CHARACTERISTIC);
    }

    service = gatt.getService(BluetoothUuids.GENERIC_ACCESS_SERVICE);
    if (service != null) {
      deviceNameCharacteristic = service.getCharacteristic(BluetoothUuids.DEVICE_NAME_CHARACTERISTIC);
    }

    service = gatt.getService(BluetoothUuids.VECTOR_VARIO_SERVICE);
    if (service != null) {
      varioCharacteristic = service.getCharacteristic(BluetoothUuids.VECTOR_VARIO_VARIO_CHARACTERISTIC);
    }

    service = gatt.getService(BluetoothUuids.ENVIRONMENTAL_SENSING_SERVICE);
    if (service != null) {
      windSpeedCharacteristic = service.getCharacteristic(BluetoothUuids.WIND_SPEED_CHARACTERISTIC);
      windDirectionCharacteristic = service.getCharacteristic(BluetoothUuids.WIND_DIRECTION_CHARACTERISTIC);
    }

    if (dataCharacteristic == null)
      throw new Error("HM10 data characteristic not found");

    if (deviceNameCharacteristic == null)
      throw new Error("GATT device name characteristic not found");

    /* varioCharacteristic is optional - log if missing but don't fail */
    if (varioCharacteristic == null)
      Log.w(TAG, "Vector Vario characteristic not found - vario data unavailable");

    /* wind characteristics are optional - log if missing */
    if (windSpeedCharacteristic == null || windDirectionCharacteristic == null)
      Log.w(TAG, "Wind characteristics not found - wind data unavailable");
  }

  private boolean doEnableNotification(BluetoothGattCharacteristic c) {
    BluetoothGattDescriptor d = c.getDescriptor(BluetoothUuids.CLIENT_CHARACTERISTIC_CONFIGURATION);
    if (d == null)
      return false;

    gatt.setCharacteristicNotification(c, true);
    d.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
    return gatt.writeDescriptor(d);
  }

  private void enableNotification(BluetoothGattCharacteristic c) {
    synchronized(enableNotificationQueue) {
      if (currentEnableNotification == null) {
        currentEnableNotification = c;
        if (!doEnableNotification(c))
          currentEnableNotification = null;
      } else
        enableNotificationQueue.add(c);
    }
  }

  private void setupCharacteristics() throws Error {
    findCharacteristics();

    /* Enable notifications for GPS data characteristic using queue */
    enableNotification(dataCharacteristic);

    /* Enable notifications for vario characteristic if available */
    if (varioCharacteristic != null) {
      enableNotification(varioCharacteristic);
    }

    /* Enable notifications for wind characteristics if available */
    if (windSpeedCharacteristic != null) {
      enableNotification(windSpeedCharacteristic);
    }
    if (windDirectionCharacteristic != null) {
      enableNotification(windDirectionCharacteristic);
    }

    portState = STATE_READY;
    stateChanged();
  }

  @Override
  public void onConnectionStateChange(BluetoothGatt gatt,
      int status,
      int newState) {
    try {
      if (BluetoothProfile.STATE_CONNECTED == newState) {
        if (!gatt.discoverServices())
          throw new Error("Discovering GATT services request failed");
      } else {
        dataCharacteristic = null;
        deviceNameCharacteristic = null;
        varioCharacteristic = null;
        windSpeedCharacteristic = null;
        windDirectionCharacteristic = null;
        lastWindSpeedCmps = 0;
        lastWindDirCentideg = 0;

        if ((BluetoothProfile.STATE_DISCONNECTED == newState) && !shutdown &&
            !gatt.connect())
          throw new Error("Received GATT disconnected event, and reconnect attempt failed");
      }

      portState = STATE_LIMBO;
    } catch (Error e) {
      error(e.getMessage());
    }

    writeBuffer.reset();
    stateChanged();
    synchronized (gattStateSync) {
      gattState = newState;
      gattStateSync.notifyAll();
    }
  }

  @Override
  public void onServicesDiscovered(BluetoothGatt gatt,
                                   int status) {
    try {
      if (BluetoothGatt.GATT_SUCCESS != status)
        throw new Error("Discovering GATT services failed");

      /* request a high MTU (usually, HM-10 chips support 23 bytes);
         postpone the setupCharacteristics() call until onMtuChanged()
         is called - we can't do both at the same time */
      setupCharacteristicsPending = true;
      gatt.requestMtu(256);
    } catch (Error e) {
      error(e.getMessage());
      stateChanged();
    }
  }

  @Override
  public void onDescriptorWrite(BluetoothGatt gatt,
                                BluetoothGattDescriptor descriptor,
                                int status) {
    synchronized(enableNotificationQueue) {
      currentEnableNotification = enableNotificationQueue.poll();
      if (currentEnableNotification != null) {
        if (!doEnableNotification(currentEnableNotification))
          currentEnableNotification = null;
      }
    }
  }

  @Override
  public void onCharacteristicRead(BluetoothGatt gatt,
      BluetoothGattCharacteristic characteristic, int status) {
    writeBuffer.beginWriteNextChunk(gatt, dataCharacteristic);
  }

  @Override
  public void onCharacteristicWrite(BluetoothGatt gatt,
      BluetoothGattCharacteristic characteristic, int status) {
    if (BluetoothGatt.GATT_SUCCESS == status) {
      writeBuffer.beginWriteNextChunk(gatt, dataCharacteristic);
    } else {
      Log.e(TAG, "GATT characteristic write failed");
      writeBuffer.setError();
    }
  }

  @Override
  public void onCharacteristicChanged(BluetoothGatt gatt,
      BluetoothGattCharacteristic characteristic) {
    if (!safeDestruct.increment())
      return;

    try {
      /* Handle GPS NMEA data from HM-10 characteristic */
      if ((dataCharacteristic != null) &&
          (dataCharacteristic.getUuid().equals(characteristic.getUuid()))) {
        if (listener != null) {
          byte[] data = characteristic.getValue();
          listener.dataReceived(data, data.length);
        }
      }

      /* Handle Vario data from Vector Vario characteristic */
      if ((varioCharacteristic != null) &&
          (varioCharacteristic.getUuid().equals(characteristic.getUuid()))) {
        if (sensorListener != null) {
          /* Value is signed 32-bit int in dm/s, convert to m/s */
          int varioDmPerSec = characteristic.getIntValue(
              BluetoothGattCharacteristic.FORMAT_SINT32, 0);
          sensorListener.onVarioSensor(varioDmPerSec / 10.0f);
        }
      }

      /* Handle Wind Speed from Environmental Sensing service */
      if ((windSpeedCharacteristic != null) &&
          (windSpeedCharacteristic.getUuid().equals(characteristic.getUuid()))) {
        /* Value is uint16 in cm/s */
        lastWindSpeedCmps = characteristic.getIntValue(
            BluetoothGattCharacteristic.FORMAT_UINT16, 0);
        reportWind();
      }

      /* Handle Wind Direction from Environmental Sensing service */
      if ((windDirectionCharacteristic != null) &&
          (windDirectionCharacteristic.getUuid().equals(characteristic.getUuid()))) {
        /* Value is uint16 in 0.01 degrees */
        lastWindDirCentideg = characteristic.getIntValue(
            BluetoothGattCharacteristic.FORMAT_UINT16, 0);
        reportWind();
      }
    } catch (NullPointerException e) {
      /* probably caused by a malformed value - ignore */
    } finally {
      safeDestruct.decrement();
    }
  }

  private void reportWind() {
    if (sensorListener != null && lastWindSpeedCmps > 0) {
      float speedMps = lastWindSpeedCmps / 100.0f;
      float dirDeg = lastWindDirCentideg / 100.0f;
      sensorListener.onExternalWind(speedMps, dirDeg);
    }
  }

  @Override
  public void onMtuChanged(BluetoothGatt gatt, int mtu, int status) {
    super.onMtuChanged(gatt, mtu, status);

    /* the MTU reported by the Android APIs is the ATT MTU, not the
       data MTU; from this reported MTU value, we have to subtract the
       ATT header size (1 byte opcode, 2 bytes handle) to get the data
       MTU */
    final int ATT_HEADER_SIZE = 3;

    if (status == BluetoothGatt.GATT_SUCCESS && mtu > ATT_HEADER_SIZE)
      writeBuffer.setMtu(mtu - ATT_HEADER_SIZE);

    if (setupCharacteristicsPending) {
      setupCharacteristicsPending = false;

      try {
        setupCharacteristics();
      } catch (Error e) {
        error(e.getMessage());
        stateChanged();
      }
    }
  }

  @Override public void setListener(PortListener _listener) {
    portListener = _listener;
  }

  @Override
  public void setInputListener(InputListener _listener) {
    listener = _listener;
  }

  @Override
  public void close() {
    safeDestruct.beginShutdown();

    shutdown = true;
    writeBuffer.reset();
    gatt.disconnect();
    synchronized (gattStateSync) {
      long waitUntil = System.currentTimeMillis() + DISCONNECT_TIMEOUT;
      while (gattState != BluetoothGatt.STATE_DISCONNECTED) {
        long timeToWait = waitUntil - System.currentTimeMillis();
        if (timeToWait <= 0) {
          break;
        }
        try {
          gattStateSync.wait(timeToWait);
        } catch (InterruptedException e) {
          break;
        }
      }
    }
    gatt.close();

    safeDestruct.finishShutdown();
  }

  @Override
  public int getState() {
    return portState;
  }

  @Override
  public boolean drain() {
    return writeBuffer.drain();
  }

  @Override
  public int getBaudRate() {
    return 0;
  }

  @Override
  public boolean setBaudRate(int baud) {
    return true;
  }

  @Override
  public int write(byte[] data, int length) {
    if (0 == length)
      return 0;

    if (portState != STATE_READY)
      return 0;

    assert(dataCharacteristic != null);
    assert(deviceNameCharacteristic != null);

    return writeBuffer.write(gatt, dataCharacteristic,
                             deviceNameCharacteristic,
                             data, length);
  }

  protected final void stateChanged() {
    PortListener portListener = this.portListener;
    if (portListener != null && safeDestruct.increment()) {
      try {
        portListener.portStateChanged();
      } finally {
        safeDestruct.decrement();
      }
    }
  }

  protected void error(String msg) {
    portState = STATE_FAILED;

    PortListener portListener = this.portListener;
    if (portListener != null && safeDestruct.increment()) {
      try {
        portListener.portError(msg);
      } finally {
        safeDestruct.decrement();
      }
    }
  }

  static class Error extends Exception {
    private static final long serialVersionUID = -2699261433374751558L;

    public Error(String message) {
      super(message);
    }

    public Error(String message, Throwable cause) {
      super(message, cause);
    }
  }
}

#include <ESP32KeyBridge.h>
#include <ESP32KeyBridgeEspBle.h>
#include <ESP32KeyBridgeEspUsbDevice.h>
#include <EspBle.h>
#include <EspUsbDevice.h>


// ============================================================
// Hardware
// ============================================================

#define RGB_LED_PIN 21
#define LED_LEVEL   24


// ============================================================
// BLE / USB / KeyBridge
// ============================================================

EspBle ble;
EspUsbDevice usbDevice;

esp32keybridge::ESP32KeyBridge bridge;

esp32keybridge::EspBleHidHostKeyboardInputAdapter keyboard(ble);
esp32keybridge::EspBleHidHostMouseInputAdapter mouse(ble);

esp32keybridge::EspUsbDeviceHidOutputAdapter pc(usbDevice);


// ============================================================
// Connection management
// ============================================================

static constexpr size_t MaximumPeers = 2;

EspBleConnectionId peers[MaximumPeers] = {};
size_t peerCount = 0;


// ============================================================
// RGB LED
// ============================================================

void setLed(uint8_t r, uint8_t g, uint8_t b)
{
  rgbLedWrite(RGB_LED_PIN, r, g, b);
}


// ------------------------------------------------------------
// 실제 BLE 연결 상태 표시
//
// BLUE   = 0대 / scanning
// YELLOW = 1대
// GREEN  = 2대
// ------------------------------------------------------------

void updateConnectionLed()
{
  if (peerCount == 0)
  {
    // BLUE
    setLed(0, 0, LED_LEVEL);
  }
  else if (peerCount == 1)
  {
    // YELLOW
    setLed(LED_LEVEL, LED_LEVEL, 0);
  }
  else
  {
    // GREEN
    setLed(0, LED_LEVEL, 0);
  }
}


// ============================================================
// BLE HID Scan
// ============================================================

void startHidScan()
{
  if (peerCount >= MaximumPeers)
    return;

  // 아직 연결되지 않은 장치를 찾는 중
  updateConnectionLed();

  EspBleScanConfig scanConfig;

  // Scan Response까지 받기 위해 Active Scan
  scanConfig.active = true;

  ble.scanner().start(scanConfig);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  // LED OFF
  setLed(0, 0, 0);

  Serial.begin(115200);


  // ==========================================================
  // USB HID Device
  // ==========================================================

  EspUsbDeviceConfig deviceConfig;

  deviceConfig.product = "XP BLE HID Bridge";

  usbDevice.begin(deviceConfig);


  // ==========================================================
  // BLE Host
  // ==========================================================

  EspBleConfig bleConfig;

  bleConfig.deviceName = "XP BLE HID Host";

  // ----------------------------------------------------------
  // Security / Bonding
  // ----------------------------------------------------------

  bleConfig.security.enabled = true;
  bleConfig.security.bonding = true;

  // 명시적으로 설정
  bleConfig.security.pairOnConnect = true;

  // GATT persistent state
  bleConfig.persistentSubscriptions = true;


  // ----------------------------------------------------------
  // BLE 시작
  // ----------------------------------------------------------

  if (!ble.begin(bleConfig))
  {
    // PURPLE = BLE initialization failure
    setLed(
      LED_LEVEL,
      0,
      LED_LEVEL
    );

    return;
  }


  // ==========================================================
  // Auto reconnect
  // ==========================================================

  // 실행 중 link drop에 대한 자동 재연결
  ble.setAutoReconnect(true);

  // HID 정보 자동 재탐색
  ble.hidHost().setAutoRediscover(true);


  // ==========================================================
  // Connected
  // ==========================================================

  ble.onConnected(
    [](const EspBleConnection &connection)
    {
      bool alreadyHeld = false;


      // 중복 connection ID 확인
      for (size_t i = 0; i < peerCount; ++i)
      {
        if (peers[i] == connection.id)
        {
          alreadyHeld = true;
          break;
        }
      }


      // 새로운 connection 등록
      if (!alreadyHeld &&
          peerCount < MaximumPeers)
      {
        peers[peerCount++] = connection.id;
      }


      // 실제 연결 대수를 LED에 반영
      updateConnectionLed();


      Serial.printf(
        "Connected %s as id %u (%u held)\n",
        connection.peerAddress.c_str(),
        static_cast<unsigned>(connection.id),
        static_cast<unsigned>(ble.connectionCount())
      );


      // ------------------------------------------------------
      // MultiConnection 공식 예제 방식
      //
      // 한 대가 연결되었으면 즉시 다음 장치 검색
      // ------------------------------------------------------

      if (peerCount < MaximumPeers)
      {
        startHidScan();
      }
    }
  );


  // ==========================================================
  // Disconnected
  // ==========================================================

  ble.onDisconnected(
    [](const EspBleConnection &connection)
    {
      // 해당 connection ID 제거
      for (size_t i = 0; i < peerCount; ++i)
      {
        if (peers[i] != connection.id)
          continue;


        peers[i] = peers[peerCount - 1];

        peerCount--;

        break;
      }


      updateConnectionLed();


      Serial.printf(
        "Disconnected id %u (%u held)\n",
        static_cast<unsigned>(connection.id),
        static_cast<unsigned>(ble.connectionCount())
      );


      // ------------------------------------------------------
      // 실행 중 연결 해제:
      //
      // AutoReconnect가 기존 peer를 복구할 수 있고,
      // 동시에 scan으로 advertising도 탐색한다.
      // ------------------------------------------------------

      startHidScan();
    }
  );


  // ==========================================================
  // Security
  // ==========================================================

  ble.onSecurityChanged(
    [](const EspBleSecurityChanged &event)
    {
      if (!event.success)
      {
        // RED = security failure
        setLed(
          LED_LEVEL,
          0,
          0
        );

        Serial.printf(
          "Security FAILED id %u\n",
          static_cast<unsigned>(
            event.connection.id
          )
        );

        return;
      }


      Serial.printf(
        "Security OK id %u\n",
        static_cast<unsigned>(
          event.connection.id
        )
      );


      // HID Report Map / Report discovery
      ble.hidHost().discover(
        event.connection.id
      );


      // Security 성공 후에는 다시
      // 실제 연결 상태 색으로
      updateConnectionLed();
    }
  );


  // ==========================================================
  // Scan Result
  // ==========================================================

  ble.scanner().onResult(
    [](const EspBleScanResult &result)
    {
      if (!result.connectable)
        return;


      if (peerCount >= MaximumPeers)
        return;


      // ======================================================
      // ★ 핵심 수정
      //
      // 일반 BLE HID advertising:
      //
      //     Service UUID 1812 있음
      //
      // 기존 bonded device의 directed advertising:
      //
      //     payload 자체가 없음
      //     따라서 1812도 없음
      //     connectable = true
      //     scannable   = false
      //
      // 기존 코드는 1812만 허용했기 때문에
      // directed advertising을 전부 버리고 있었음.
      // ======================================================

      bool normalHidAdvertisement =
        result.advertisesService("1812");


      bool possibleDirectedAdvertisement =
        !result.scannable;


      // 둘 중 어느 것도 아니면 관심 없는 장치
      if (!normalHidAdvertisement &&
          !possibleDirectedAdvertisement)
      {
        return;
      }


      // ------------------------------------------------------
      // 연결 전에 scan 정지
      // ------------------------------------------------------

      ble.scanner().stop();


      // ------------------------------------------------------
      // 일반 HID든 directed reconnect든
      // scanResult에는 connect에 필요한
      // address / addressType이 들어 있음.
      // ------------------------------------------------------

      if (!ble.connect(result))
      {
        // 연결 요청 자체가 실패하면 다시 scan
        startHidScan();
      }
    }
  );


  // ==========================================================
  // ESP32KeyBridge
  // ==========================================================

  bridge.addInput(keyboard);
  bridge.addInput(mouse);

  bridge.addOutput(pc);


  esp32keybridge::ESP32KeyBridgeConfig bridgeConfig;

  bridge.applyConfig(bridgeConfig);


  // ==========================================================
  // Cold boot
  //
  // bond는 이미 NVS에서 복원되어 있음.
  //
  // 일반 1812 advertising뿐 아니라
  // bonded peripheral의 directed advertising도 기다린다.
  // ==========================================================

  startHidScan();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  ble.update();

  bridge.update();

  delay(1);
}
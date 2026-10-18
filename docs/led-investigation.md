# Reviung41 底燈 (RGB) 與電源調查紀錄

日期：2026-10-18
狀態：**暫停中**。LED 不常開，目前以 FW 限制亮度（`BRT_MAX=30`）因應，硬體維修待日後決定。

---

## 1. 結論摘要

| 項目 | 結論 | 把握程度 |
|---|---|---|
| nRF52840 DC/DC（REG0 + REG1） | 兩顆電感都有，USB 與電池 + BLE 下都穩定，已在正式設定開啟 | 高（實測） |
| LED 電源開關（P0.13 / EXT_POWER） | 存在且正常，RGB 關閉時 VCC 確實斷電 | 高（實測） |
| 全體 LED 亂閃 | 與 DC/DC **無關**（LDO 版本也會亂閃）。最可能是 WS2812B 以 3.3 V 運作（低於規格 3.5 V）加上沒有去耦電容，高亮度時更明顯 | 中（推論，待負載電壓量測確認） |
| L2、L9 不亮 | LED 發光部分損壞（資料仍有往後傳） | 高 |
| 正面 L11 不會熄滅 | 這顆在 3.3 V 下解碼不穩，或本身損壞。它是鏈的最後一顆，不影響其他 LED | 中 |
| LED 問題會不會影響按鍵 | 不會。電氣上和矩陣分開；亂閃時打字正常 | 高 |

---

## 2. 硬體背景

### 2.1 原始設計
- 來源：<https://github.com/gtips/reviung/tree/master/reviung41>（本板為修改版，以 ver1.4 的 netlist 對照）
- LED：**WS2812B（5050, PLCC4）× 11**，規格 VDD **3.5–5.3 V**
- 原設計主控為 5 V 的 Pro Micro（ATmega32U4），LED 接主控 **VCC（pin 21）**
- **原設計沒有任何去耦電容**（WS2812B datasheet 建議每顆 100 nF）

### 2.2 實際主控
- SuperMini 型「ProMicro NRF52840」（nice!nano 相容）
- LED 資料線：pin 1（D1/TX）= **P0.06**
- VCC = nRF 的 VDD（REG0 輸出 3.3 V）經 **P0.13** 控制的開關輸出
- 所以 LED 實際以 **3.3 V** 運作，低於 WS2812B 規格

### 2.3 資料鏈順序（ver1.4 netlist）

```
主控 P0.06 → L1 → L2 → L3 → L4 → L5 → L6 → L7 → L8 → L9 → L10 → L11（正面，最後一顆）
```

背面視角（USB 線在上方）的位置：

| LED | 位置（背面視角） | 狀態 |
|---|---|---|
| L1 | 主控旁、中間偏右上 | ✅ |
| **L2** | 右上 | ❌ 不亮 |
| L3 | 最右側 | ✅ |
| L4 | 右下 | ✅ |
| L5、L6 | 底部中間 | ✅ |
| L7 | 左下 | ✅ |
| L8 | 最左側 | ✅ |
| **L9** | 左上 | ❌ 不亮 |
| L10 | 中間偏左上 | ✅ |
| **L11** | 正面（PCB 上面，F.Cu） | ⚠️ 不會熄滅 |

> 待確認：測試時記錄的「正面在 WALK #10 亮綠」和 netlist（正面是 #11）差一號。可能是 L9 不亮，那一步漏數了；也可能修改版改過順序。下次用 `ledtest` 時看 serial log 的 `WALK: LED #n` 確認。

### 2.4 腳位與量測點（主控，元件面、USB-C 朝上）
- **VCC**：左排，RST 下方那腳（LED 電源，經開關）
- **VDD**：左下 SWD 焊盤最上面（REG0 輸出，不經開關）
- **GND**：右排標「GND」兩腳，或 SWD 焊盤最下面
- **RAW**：左排最上面。在這片板子上是**電池電壓**，不是 USB 5 V
- LED 的 VDD 腳：斷電時用導通檔，從主控 VCC 找會嗶的那腳

---

## 3. 測試與量測結果

### 3.1 nRF52840 電源暫存器（`pwrdiag-C-dcdc-both`）
| 暫存器 | 值 | 意義 |
|---|---|---|
| MAINREGSTATUS | 1 | High-voltage 模式：電池/USB 進 VDDH，REG0 在電源路徑上 |
| DCDCEN / DCDCEN0 | 1 / 1 | REG1、REG0 都是 DC/DC |
| UICR REGOUT0 | 5 → 3.3 V | VDD 設定正確 |
| RESETREAS | 0x1（reset pin） | 刷機造成，作為基準 |

- USB：143 s 正常
- 拔 USB，電池 + BLE 打字約 15 分鐘（RGB 開）：uptime 1023 s 連續、RESETREAS 不變 → **沒有重置**
- 結論：兩顆 DC/DC 電感都有。正式設定加上 `CONFIG_BOARD_ENABLE_DCDC_HV=y`（v0.3 預設 REG0 是 LDO）

### 3.2 LED 電源開關（EXT_POWER）
- RGB 開著時按 EP OFF → LED 立即全滅；EP ON → 立即恢復
- 結論：P0.13 開關存在。RGB 關閉時 LED 完全不耗電

### 3.3 電壓量測（接 USB，低負載）
| 點 | 值 |
|---|---|
| VCC（LED 關） | 0 V（量不到） |
| VCC（LED 開） | 3.31 V |
| VDD | 3.28 V |
| RAW | 4.17 V（電池充電中） |

VCC ≈ VDD，開關上幾乎沒有壓降。

**尚未完成：高亮度下的電壓。** 刷 `ledtest-A-ldo`，在白色分段時記錄：

| 亮度 | 主控 VCC | 主控 VDD | 最遠 LED（L5/L6 或 L3/L8）的 VDD |
|---|---|---|---|
| 全暗 | | | |
| 2% | | | |
| 10% | | | |
| 25% | | | |
| 50% | | | |
| 100% | | | |

判讀：
- VCC 和 VDD 一起掉 → REG0 供電能力不足（nRF 對外負載大約 25 mA）
- 只有 VCC 掉 → 開關 MOSFET 壓降
- 主控端穩、LED 端低 → PCB 電源走線壓降
- 全部穩 → 單純是 WS2812B 在 3.3 V 下不穩

### 3.4 LED 現象（`ledtest`）
- WALK（低亮度逐顆點亮）：L1 紅色正確，L3–L8、L10 依序綠色正確
- L2、L9：輪到時不亮，但後面的 LED 能正確收到資料 → **發光部分損壞**
- 正面 L11：輪到時亮正確的綠色，其他時間（包括全暗）一直亮淡紫白；前一顆亮時變洋紅
- 全體亂閃：所有 LED（包括 L1）一起快速換色，A-ldo 版本也會出現 → 與 DC/DC 無關，源頭在供電或資料線源頭
- 白色看起來偏粉紫：3.3 V 下綠、藍晶片的電壓餘量不足
- 亂閃時打字正常 → MCU 的供電不受影響

---

## 4. 處理選項

| 方案 | 作法 | 效果 / 代價 | 狀態 |
|---|---|---|---|
| A. FW 限亮度 | `CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX=30` | 減少電流與掉壓；不改變 3.3 V 在規格外的事實 | ✅ 已做 |
| B. 補電容 | VCC 進入點加 10 µF；問題 LED 的 VDD/GND 焊 100 nF | 降低雜訊；PCB 沒有焊盤，要飛線 | 未做 |
| C. 換 LED | 換 L2、L9；L11 視情況 | 修好不亮的兩顆；L11 換了不一定會好 | 未做 |
| D. LED 改用 VBUS 5 V | 切斷 LED 的 VCC 走線，改接 USB-C 的 VBUS | LED 回到規格內，顏色和亮度正常。只有接 USB 時有電（符合使用習慣）。3.3 V 資料驅動 5 V 的 WS2812B 不保證（VIH = 3.5 V），需要 level shifter（例如 74AHCT1G125），或讓第一顆 LED 經二極體供電（約 4.3 V）兼做電平轉換 | 未做 |

補充：
- **換「3 V 規格」的 LED**：只能改善解碼穩定性；顏色偏差（綠、藍的順向電壓）和 REG0 的供電能力限制不會改變。目前沒找到確認可直接替換的 5050、3.3 V 型號。
- **RAW 不適合供電給 LED**：RAW 是電池電壓（3.0–4.2 V），低電量時仍低於規格，而且不經過開關，休眠時也會耗電。

### 換零件注意事項
- 型號：WS2812B 5050 PLCC4，換之前對照拆下來的原件
- 方向：看缺角或 GND 標記，裝反會讓後面整條鏈不亮
- 溫度：低溫、短時間（大約 300 °C 以下），周邊用高溫膠帶或鋁箔保護
- 周邊：焊盤旁有熱插拔座和 1N4148，換完要**測全部按鍵**

---

## 5. 目前的韌體設定（branch `tune-hrm-rgb-v0.3`）
- ZMK 釘在 **v0.3**（`config/west.yml`、`.github/workflows/build.yml`），board 用 `nice_nano_v2`
- RGB：`BRT_MAX=30`、`BRT_START=30`（實際輸出約 9%，BRI+ 調到頂是 30%）
- `rgb_battery_guard` 模組（`src/rgb_battery_guard.c`）：用電池時強制關 RGB 並切斷 LED 電源；接 USB 時手動控制，並恢復拔線前的狀態
  - 不用 ZMK 內建的 `AUTO_OFF_USB` / `AUTO_OFF_IDLE`：v0.3 中兩者共用狀態，可能在電池模式下把 RGB 重新打開
- DC/DC：`CONFIG_BOARD_ENABLE_DCDC=y`、`CONFIG_BOARD_ENABLE_DCDC_HV=y`

---

## 6. 測試工具
原始碼在 `docs/tools/pwrdiag/` 和 `docs/tools/ledtest/`，各自是完整的 ZMK extra module（用 `-DZMK_EXTRA_MODULES=<該資料夾>` 載入）。測試 FW 的 `.uf2` 在 `firmware/`，不納入 git，需要時可重新 build。

| 工具 | 用途 | build 方式 |
|---|---|---|
| `pwrdiag` | 每 10 秒從 USB log 印出 MAINREGSTATUS、DCDCEN、DCDCEN0、REGOUT0、USBREGSTATUS、RESETREAS | 當作 ZMK extra module，加 `-S zmk-usb-logging`；用 `CONFIG_BOARD_ENABLE_DCDC(_HV)` 切換 A/B/C 版本 |
| `ledtest` | 不經過 ZMK underglow 直接驅動 LED：WALK（逐顆、#1 紅、其餘綠）→ 全暗 → 白色 2/10/25/50/100% → 全暗 | 當作 extra module；設定 `CONFIG_ZMK_RGB_UNDERGLOW=n`、`CONFIG_LED_STRIP=y`、`CONFIG_WS2812_STRIP=y`、`CONFIG_SPI=y`，keymap 的 `&rgb_ug` 換成 `&none`；`CONFIG_LEDTEST_WALK_ONLY=y` 只跑低亮度 |

看 log：`ls /dev/cu.usbmodem*` → `screen /dev/cu.usbmodemXXXX 115200`（離開：Ctrl-A、K）

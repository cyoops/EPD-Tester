# EPD-Tester
---
自用的墨水屏测试程序，支持自己已经拥有的墨水屏。可以使用不同的MCU驱动，包括esp32系列，esp8266和Arduino。


基础目录和主要文件：

- `platformio.ini`：PlatformIO 环境和编译宏。
- `src/config.h`：MCU 引脚、屏幕尺寸、帧缓冲区和运行时常量。
- `src/epd_driver.cpp`：所有墨水屏驱动的实现和编译分支。
- `src/epd_driver.h`：驱动层对上层提供的统一接口。
- `src/display.cpp`：本地测试画面的绘制和轮播。
- `src/main.cpp`：串口、GPIO、屏幕初始化及全屏刷新的启动流程。

只做点亮和检测！！！



---

## 1. 项目硬件和软件基础

### 1.1 MCU 接线和板级 profile

所有 MCU 引脚映射都由 `src/config.h` 中的 `BOARD_PROFILE_*` 决定；
屏幕型号宏通常不改变这些 GPIO；双 SSD1608 面板需要额外的 CS1。下表列出
项目已定义的全部 profile。



#### 墨水屏信号

| 板级 profile | PlatformIO board | MOSI/DIN | SCK/CLK | CS | DC | RST | BUSY | CS1 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| `BOARD_PROFILE_ESP32_C3` | `esp32-c3-devkitm-1` | 6 | 4 | 7 | 1 | 2 | 10 | 5 |
| `BOARD_PROFILE_ESP32_C3_WROOM02` | `esp32-c3-devkitm-1` | 6 | 4 | 7 | 1 | 2 | 10 |
| `BOARD_PROFILE_ESP32_WROOM32E` | `esp32dev` | 14 | 13 | 15 | 27 | 26 | 25 |
| `BOARD_PROFILE_SMT_WROOM32E` | `esp32dev` | 14 | 13 | 15 | 27 | 26 | 25 |
| `BOARD_PROFILE_SMT_C3` | `esp32-c3-devkitm-1` | 6 | 4 | 7 | 1 | 2 | 10 |
| `BOARD_PROFILE_YD_ESP32_S3_N16R8` | `esp32-s3-devkitc-1` | 11 | 12 | 10 | 9 | 8 | 7 |




### 1.2 默认屏幕参数

默认屏幕尺寸是 400 x 300：

```text
EPD_WIDTH  = 400
EPD_HEIGHT = 300
```

单色帧缓冲区大小为：

```text
IMG_BUF_LEN = (EPD_WIDTH / 8) * EPD_HEIGHT
             = 15000 bytes
```

像素采用每行连续存储、每字节 8 个像素、最高位先发送：

- `1`：白色
- `0`：黑色

2bpp 图像每字节包含 4 个像素，大小为 30000 bytes。设置
`-DEPD_BPP=2` 后，网络下载和上层显示逻辑会启用 `colorBuf`；这表示
数据传输格式，不保证面板本身一定能显示红色等多种颜色。

灰度或多色墨水屏按需定义更大的：DEPD_BPP值



### 1.3 软件环境

项目使用：

- PlatformIO
- Arduino framework
- Espressif 32 platform
- ESP32-C3 DevKitM-1 等 ESP32 系列板卡
- GxEPD2（只用于部分硬件 SPI 面板）

通用设置位于 `platformio.ini` 的 `[common]`，具体环境通过 `extends =
common` 继承。

---


## 2. 配置命名和 `platformio.ini`

### 2.1 配置名约定

配置名统一使用小写字母，并由以下部分组成：

```text
epd_<尺寸>_<控制器或驱动>_<颜色>_<MCU板型>
```

示例：

```text
epd_42_ssd1683_bw_c3_supermini
epd_42_ssd1619a_bw_c3_supermini
epd_42_kegm042001t06_bwr_c3_supermini
```
含义如下：

- `42`：4.2 英寸。
- `ssd1683`、`ssd1619a`：驱动控制器；面板专用协议可使用面板/驱动名。
- `bw`：黑白。
- `bwr`：黑白红。
- `c3_supermini`、`c3_std`、`c3_wroom02`、`wroom32e`、
  `smt_wroom32e`、`yd_s3_n16r8`：板型和对应上传/USB配置。当前仅启用
  `c3_supermini`；其他板型环境已在 `platformio.ini` 中注释保留。

新增屏幕时必须先查找最接近的通用驱动配置，使用该配置编译、刷写并做实物验证。
验证通过后，将屏幕型号和验证结果加入该配置的兼容清单，不新增环境；
只有实测失败且确认需要不同初始化序列、LUT、RAM 布局、刷新策略或引脚时，
才新增独立配置，并说明与通用配置不兼容的原因。只凭控制器型号相同不能
判定兼容。

### 2.2 新增一个已有 MCU 的屏幕配置

如果新屏幕属于已验证的 4.2 英寸 SSD1683 软件 SPI 兼容面板，先使用通用配置：

```sh
pio run -e epd_42_ssd1683_bw_c3_supermini
pio run -e epd_42_ssd1683_bw_c3_supermini -t upload
pio device monitor -e epd_42_ssd1683_bw_c3_supermini
```

只有通用配置经刷写实测确认不兼容后，才按以下形式增加新环境：

```ini
[env:epd_42_example_bw_c3_supermini]
extends = common
board = esp32-c3-devkitm-1
build_flags =
    -DBOARD_PROFILE_ESP32_C3
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DEPD_WIDTH=400
    -DEPD_HEIGHT=300
    -DEPD_PANEL_42_EXAMPLE_BW
    -DEPD_BPP=1
    -DALLOW_INSECURE_FALLBACK=0
```

字段作用：

- `board`：PlatformIO 的 MCU/开发板定义。
- `BOARD_PROFILE_*`：选择 `src/config.h` 中的引脚组。
- `ARDUINO_USB_*`：C3 USB CDC 和串口启动方式。
- `EPD_WIDTH`、`EPD_HEIGHT`：屏幕控制器 RAM 窗口和本地缓冲区尺寸。
- `EPD_PANEL_*`：选择 `src/epd_driver.cpp` 的驱动分支。
- `EPD_BPP=1`：1bpp 黑白数据路径。
- `EPD_BPP=2`：2bpp 数据路径；通常用于彩色屏，也可用于需要特殊
  plane 映射的黑白面板。
- `ALLOW_INSECURE_FALLBACK`：沿用项目安全配置，不要随意打开。

如果新屏幕只是同一控制器的不同批次，优先新增一个环境并复用已验证
的驱动宏；不要为了改一个配置名复制一套驱动。

### 2.3 新增 MCU 与屏幕组合

新增 MCU 时需要同时检查：

1. 在 `src/config.h` 增加 `BOARD_PROFILE_*`。
2. 定义 MOSI、SCK、CS、DC、RST、BUSY、ADC、按键和 LED。
3. 确认 GPIO 没有和 USB、音频、启动 strap 或其他外设冲突。
4. 在 `platformio.ini` 新增对应环境。
5. 确认 `board`、Flash 模式、Flash 频率、分区表和上传速度。
6. 如果是 ESP32-S3 或带 PSRAM 的板卡，补充 PSRAM 和对应分区配置。
7. 编译新环境，并至少编译一个原有环境确认没有破坏公共源文件。

不要只修改 `platformio.ini` 中的引脚宏。引脚最终来自
`src/config.h` 的 `BOARD_PROFILE_*`，配置文件只负责选择 profile。

### 2.4 修改现有配置

修改前先确认修改目标：

- 只改环境名称：修改 `[env:...]` 名称和相关注释。
- 改屏幕控制器：修改 `EPD_PANEL_*`，并检查驱动分支。
- 改屏幕尺寸：同时修改 `EPD_WIDTH`、`EPD_HEIGHT`、服务器请求和驱动
  的 RAM window。
- 改 MCU：同时修改 `board`、`BOARD_PROFILE_*` 和 USB/Flash 设置。
- 改颜色能力：检查 `EPD_BPP`、网络参数、`colorBuf` 和
  `epdDisplay2bpp()`。

配置名修改不会自动改变控制器驱动。比如把 `bwr` 改成 `bw`，如果
`EPD_PANEL_*` 和 `EPD_BPP` 不变，运行行为仍然可能是原来的彩色驱动。

### 2.5 通用驱动兼容范围

只有相同驱动分支、面板设置和 RAM 布局均已确认的型号才合并为通用环境；
仅共享控制器芯片名称不足以判定兼容。

| 通用配置 | 已知兼容屏幕/限制 |
|---|---|
| `epd_42_ssd1683_bw_*` | Waveshare 4.2 英寸 V2 黑白屏（WSV2，400×300）和中景园 4.2 英寸黑白屏（400×300），使用同一 SSD1683 软件 SPI 初始化/全刷驱动。其他 SSD1683 屏幕不能只凭控制器型号推定兼容，需核对 RAM 尺寸、面板设置、BUSY 极性和 LUT。 |
| `epd_42_wft042_bw_*` / `epd_42_wft042_bwr_*` | Waveshare WFT0CZ15 系列及 WFH0420CZ35 系列使用相同 WFT042 兼容驱动。WFH0420CZ35 当前 2bpp 路径中逻辑红色显示为黑，颜色映射须以实物复核。 |
| `epd_583_gdeq0583t31_uc8179_bw_*` | GDEQ0583T31 / UC8179 5.83 英寸屏，648×480；使用同一 GxEPD2 面板类并固定整屏刷新以避免局刷残影。 |

GDEY042T81 虽然使用 SSD1683，但该环境通过 GxEPD2 实现，和 SSD1683 软件
SPI 通用配置不是同一驱动路径，故保留专用配置。HINK-E042A31 的控制器型号
尚未确认，也保留专用配置。

HINK-E0213A127（250×128，SSD1680）目前使用专用候选环境
`epd_213_hink_e0213a127_ssd1680_bwr_c3_supermini`，复用 GxEPD2
`GxEPD2_213_Z98c` 三色驱动，并将横向测试图转换为控制器的 128×250 原生方向。
该驱动类标注的可视宽度为 122 像素；HINK 屏的实际可视区、初始化波形、
方向和颜色映射尚未实物确认，因此它是待验证的首次测试配置，不代表已兼容
其他 SSD1680 屏幕。刷写后应检查图像方向、边缘裁切及黑/白/红显示，再决定
是否将此型号列为已验证兼容屏；若驱动或面板设置不兼容，再为其增加专用分支。

HINK-E029A01（2.9 英寸，296×128，SSD1608）已在本项目实物验证，可使用通用环境
`epd_29_ssd1608_bw_c3_supermini`。该环境使用 GxEPD2 的 2.9 英寸 V1
`GxEPD2_290` 驱动，并将横向测试画面转换为 128×296 原生方向；本项目已确认屏幕
正常显示。此兼容结论仅适用于已测试的 HINK-E029A01，不应仅凭分辨率或控制器名称
推定其他屏幕兼容。原 `epd_29_gdey029t94_bw_c3_supermini` 使用 GDEY029T94 /
SSD1680 驱动，仍独立保留。

A1360069（标称 212×104，推测 IL0373）使用独立候选环境
`epd_213_a1360069_il0373_bwr_c3_supermini`，采用 GxEPD2
`GxEPD2_213c` 三色驱动（该类面板参数为原生 104×212），并转换横向 212×104
测试画面。由于控制器和颜色类型尚未由屏幕资料确认，此配置仅为待实物验证的
BWR 候选，不据此宣称兼容其他 IL0373 面板。若该屏实际为黑白版，需要按其准确
型号和驱动路径另行选择配置。

这两个 2.13 英寸候选环境共用 ESP32-C3 DevKitM-1 板级配置和
`${common.c3_supermini_flags}`；PlatformIO 中已复用这部分设置。屏幕侧仍保留独立
配置：HINK-E0213A127 使用 SSD1680、250×128，A1360069 候选使用 IL0373、
212×104。控制器、RAM 布局和初始化/波形参数不同，因此不能合并为同一个面板
驱动配置；只有实测确认面板设置及显示行为兼容后，才应扩展兼容清单。

### 2.6 所有已声明的 PlatformIO 显示配置

以下清单列出项目支持的面板配置，不代表每种面板和 MCU 组合都已完成实物验证。
当前仅启用名称以 `c3_supermini` 结尾的环境；其他板型环境已在
`platformio.ini` 中注释保留，需要使用时再解除相应环境注释。

| 面板 / 驱动路径 | PlatformIO 环境（含 MCU 或功能变体） | 数据路径 |
|---|---|---|
| 4.2 英寸 SSD1683 软件 SPI | `epd_42_ssd1683_bw_c3_supermini`、`epd_42_ssd1683_bw_c3_std`、`epd_42_ssd1683_bw_wroom32e`、`epd_42_ssd1683_bw_yd_s3_n16r8`、`epd_42_ssd1683_bw_wroom32e_ai_chat`、`epd_42_ssd1683_bw_wroom32e_vocab_review` | 1bpp；兼容屏见上表 |
| GYE042A87 / GxEPD2 | `epd_42_gye042a87_bw_c3_supermini`、`epd_42_gye042a87_bw_c3_std`、`epd_42_gye042a87_bw_wroom32e`、`epd_42_gye042a87_bw_smt_wroom32e` | 1bpp |
| GDEM042F52 / JD79668，专用软件 SPI | `epd_42_gdem042f52_jd79668_bwr_c3_supermini`、`epd_42_gdem042f52_jd79668_bwr_c3_std`、`epd_42_gdem042f52_jd79668_bwr_wroom32e` | 2bpp |
| KEGM042001T06 / P420016，BWR | `epd_42_kegm042001t06_bwr_c3_supermini` | 2bpp，专用黑/红 plane 映射 |
| WFT042 兼容驱动 | `epd_42_wft042_bw_c3_supermini`、`epd_42_wft042_bw_c3_std`、`epd_42_wft042_bwr_c3_supermini`、`epd_42_wft042_bwr_c3_wroom02`、`epd_42_wft042_bwr_c3_std`、`epd_42_wft042_bwr_wroom32e` | BW 1bpp / BWR 2bpp |
| HINK-E042A13-A0 / SSD1619A | `epd_42_ssd1619a_bw_c3_supermini` | 1bpp；已验证黑白显示及上下方向 |
| HINK-E042A03，双 SSD1608 | `epd_42_ssd1608_dual_bw_c3_supermini` | 1bpp；双 CS、分半写入、显式 LUT |
| HINK-E042A31(-A1)，三色 | `epd_42_hink_e042a31_bwr_c3_supermini` | 2bpp；控制器型号仍未核实，保留专用配置 |
| HINK-E0213A127 / SSD1680，三色（候选） | `epd_213_hink_e0213a127_ssd1680_bwr_c3_supermini` | 250×128 输入，2bpp；使用 128×250 原生方向的 GxEPD2 驱动，待实物验证 |
| A1360069 / IL0373，三色（候选） | `epd_213_a1360069_il0373_bwr_c3_supermini` | 212×104 输入，2bpp；使用 GxEPD2 213c 104×212 原生方向驱动，待实物验证 |
| DEPG0420 / RY683，专用软件 SPI | `epd_42_depg0420_ry683_bwr_c3_supermini` | 2bpp；专用 DKE RY683 分支 |
| 2.9 英寸，GDEY029T94 / SSD1680 / GxEPD2 | `epd_29_gdey029t94_bw_c3_supermini`、`epd_29_gdey029t94_bw_wroom32e` | 296×128，1bpp |
| 2.9 英寸 SSD1608 / V1 | `epd_29_ssd1608_bw_c3_supermini` | 296×128，1bpp；已验证 HINK-E029A01 |
| 5.83 英寸，GDEQ0583T31 / UC8179 | `epd_583_gdeq0583t31_uc8179_bw_c3_supermini`、`epd_583_gdeq0583t31_uc8179_bw_c3_std`、`epd_583_gdeq0583t31_uc8179_bw_c3_wroom02`、`epd_583_gdeq0583t31_uc8179_bw_wroom32e` | 648×480，1bpp；整屏刷新 |
| 7.5 英寸 / GxEPD2 750_T7 | `epd_75_gdey075t7_bw_c3_supermini`、`epd_75_gdey075t7_bw_wroom32e` | 800×480，1bpp |
| GDEY042T81 / GxEPD2 | `epd_42_gdey042t81_bw_c3_supermini` | 400×300，1bpp；独立 GxEPD2 路径 |
| GDEW042T2 / IL0398，GxEPD2 | `epd_42_gdew042t2_il0398_bw_c3_supermini` | 400×300，1bpp |
| GDEW042M01 / UC8176，GxEPD2 | `epd_42_gdew042m01_uc8176_bw_c3_supermini` | 400×300，1bpp |

所有环境的串口监视器默认 115200 baud；`c3_supermini` 启用 USB CDC，并在
启动时等待连接最多 1500 ms。上传默认 460800 baud（`c3_std` 使用非 USB
CDC 启动配置）。使用 `pio run -e <环境> -t upload` 刷写；需要查看连接、
擦除/写入和校验步骤时，添加 `-v`。固件串口会输出板型、屏幕、缓冲区、
GPIO、控制器初始化、全屏数据传送及刷新调用阶段和耗时。

## 3. 驱动架构

- `src/config.h` 根据 `BOARD_PROFILE_*` 选择引脚，并校验屏幕尺寸与
  `EPD_BPP`；图像缓冲区按行排列，单色像素高位先存。
- `src/epd_driver.h` 定义统一的初始化、单色/2bpp 全屏刷新和休眠接口；
  `src/epd_driver.cpp` 根据 `EPD_PANEL_*` 编译对应的已验证驱动分支。
- `src/display.cpp` 生成不依赖网络或文件系统的本地测试画面。2bpp 帧缓冲
  以每像素 2 位、每字节 4 像素（高位在前）存储；1bpp 面板把彩色索引映射
  为黑白，以便仍可验证尺寸、方向及布局。
- `src/main.cpp` 分配缓冲区并执行显示轮播；测试图由屏幕尺寸动态绘制，
  不需要把 `xbpp_img` 中所有参考文件复制到设备闪存。



## 4. 显示调用链和数据流

### 4.1 启动和初始化

`src/main.cpp` 的离线启动流程为：

```text
setup()
  -> Serial.begin(115200) / 等待 USB CDC（最多 1500 ms）
  -> gpioInit()
  -> epdInit()
  -> renderTestSlide()
  -> epdDisplay() / epdDisplay2bpp()
```

### 4.2 测试画面

固件依次显示调色板/棋盘图案、黑/白/黄/红纯色测试（中心标注颜色与
分辨率）以及带边框、十字线和方向标记的测试；方向文字放在独立区域，
不覆盖中心轴线或红/黑方向标记。
每张画面完成刷新后等待 10 秒再进入下一张。纯色画面的中心标签不覆盖
主要测试区域，同时便于辨认当前测试颜色。

`xbpp_img/` 保存与六张轮播画面对应的裸 `.1bpp` / `.2bpp` 帧缓冲及
逐文件 PNG 预览，覆盖项目使用的屏幕分辨率和黑白/三色数据路径。固件仍动态
生成测试画面，不会从该目录读取图片；这些文件用于离线核对帧缓冲和预览。
格式、色号及各分辨率的文件清单见 [xbpp_img/README.md](./xbpp_img/README.md)。

### 4.3 显示与刷新策略

- 初始化后进行全屏刷新；不执行联网、局部刷新或休眠流程。
- 按调色板、四种纯色和方向/布局测试顺序循环，刷新结束后再计时等待
  10 秒。
- 默认 PlatformIO 环境为 `epd_42_ssd1683_bw_c3_supermini`，使用
  ESP32-C3 DevKitM-1、默认 400 x 300 分辨率与 SSD1683 软件 SPI 驱动。
- `epd_42_wft042_bwr_c3_supermini` 提供 2bpp 三色驱动的编译验证环境。

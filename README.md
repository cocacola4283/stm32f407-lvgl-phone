# STM32F407 LVGL 小手机 (LVGL Phone)

> 一个跑在 **STM32F407VET6 + 2.8" 320×240 电阻触摸屏** 上的"类手机"嵌入式系统：
> 图形界面用 **LVGL 9.5**，带桌面图标、小游戏、SD 卡文件浏览器和 JPEG 图片浏览器，
> 并针对无 GPU 的 MCU 做了一整套性能榨取（DMA 双缓冲刷屏 + JPEG 流式解码）。

![platform](https://img.shields.io/badge/MCU-STM32F407VET6-blue)
![ui](https://img.shields.io/badge/GUI-LVGL%209.5-green)
![toolchain](https://img.shields.io/badge/toolchain-Keil%20MDK%20ARMCC%205-orange)
![flash](https://img.shields.io/badge/flash-342%20KB%20%2F%20512%20KB-lightgrey)
![ram](https://img.shields.io/badge/RAM-100%20KB%20%2F%20128%20KB-lightgrey)

---

## 功能

| 功能 | 状态 | 说明 |
|---|---|---|
| 桌面（iOS 风格图标） | ✅ | 2 行 × 3 列圆角渐变图标，按下变暗反馈，瞬时切换 |
| **2048 小游戏** | ✅ | 数字块滑动动画（`lv_anim` 缓动）+ 计分 + 合并 + 手势/方向键 + GAME OVER |
| **SD 卡文件浏览器** | ✅ | 自己实现的 FAT16/FAT32 只读驱动，列目录、显示大小、可滚动、可刷新 |
| **JPEG 图片浏览器** | ✅ | TJpgDec **流式解码**（16 KB 而不是 150 KB），全屏显示、左右翻页 |
| TO DO / 设置 / 关于 | 🚧 | 占位页，接口已留好 |
| 触摸校准持久化 | ✅ | 校准值存 **MCU 内部 Flash**（这块板没有 24C02 EEPROM） |
| 软复位 | ✅ | 屏幕 RST 按钮（板载 RESET 键损坏） |

## 实测性能

| 场景 | FPS | CPU |
|---|---|---|
| 桌面静止 | 49~53 | 3~6% |
| 2048 滑动 | **40+** | **30~40%**（DMA 流水线之前是 20~30 fps / 80%） |
| 快速连点 | 40+ | 60~80%（触摸扫描 + 渲染排队，正常） |

## 资源占用

| | 用量 | 上限 | 剩余 |
|---|---|---|---|
| **Flash** | 342 KB | 512 KB | 170 KB |
| **RAM** | 100 KB | 128 KB | 28 KB |

RAM 明细：显示双缓冲 40 KB + LVGL 内存池 48 KB + 栈 8 KB + 其它约 4 KB

## 硬件

- **MCU**：STM32F407VET6（168 MHz，512 KB Flash，128 KB RAM，带 FPU）
- **屏幕**：2.8" TFT，320×240，**FSMC 16 位并口 8080**（NE4 + A6 作 RS）
- **触摸**：XPT2046 电阻屏（SPI），需 5 点校准
- **存储**：板载 MicroSD（SDIO 4 位：PC8~PC12 + PD2）、W25Qxx SPI Flash
- **时钟**：HSE 8 MHz → PLL（M=8, N=336, P=2, Q=7）→ 168 MHz / 48 MHz（SDIO 需要）

## 目录结构

```
LVGL_PORTING/
├── Drivers/
│   ├── BSP/
│   │   ├── LCD/             # FSMC 并口屏驱动
│   │   ├── TOUCH/           # 触摸（tp_cal.c：校准值存内部 Flash）
│   │   ├── SD/
│   │   │   ├── sd_card.c    # SDIO + HAL SD
│   │   │   ├── fat_ro.c     # 自写 FAT16/32 只读驱动（含文件句柄 API）
│   │   │   ├── lv_fs_fat.c  # 把 fat_ro 注册成 LVGL 的 S: 盘
│   │   │   └── photo_view.c # JPEG 流式解码 + 逐带写屏
│   │   ├── KEY/ IIC/ 24CXX/ SRAM/ TIMER/ LED/
│   ├── STM32F4xx_HAL_Driver/
│   └── CMSIS/
├── Middlewares/LVGL/LVGL_SRC/lvgl/    # LVGL 9.5 源码（裁剪配置见 lv_conf.h）
├── GUI/
│   ├── apps/                # 桌面 shell + 文件浏览器 + 图片浏览器 + 内嵌图标
│   ├── game2048/            # 带动画的 2048（固定对象池，零内存 churn）
│   └── generated/ custom/   # GUI Guider 生成代码（当前未调用，链接器自动剔除）
├── User/                    # main.c / 中断 / HAL 配置
├── tools/
│   ├── gen_icons.py         # PC 端生成 iOS 风格图标（PNG + LVGL .bin + C 数组）
│   ├── gen_test_jpg.py      # 生成 JPEG 测试图
│   └── icons_preview/       # 图标预览（PNG）
└── Projects/MDK-ARM/        # Keil 工程
```

## 编译与烧写

1. 用 **Keil MDK** 打开 `Projects/MDK-ARM/stm32f407_lvgl.uvprojx`
2. 编译（F7）。工程用 **ARMCC v5**，`-O3` 优化
3. 烧写：**不要用 Keil 的下载按钮**（这块板下载后会跑飞），用 `flash.bat`

```
flash.bat "D:\path\to\stm32f407_lvgl.uvprojx"
```
脚本会：ST-LINK_CLI 编程 → 重连 → Run → 回读 PC 自检，每步都验证。

## 关键技术点

### 1. DMA 双缓冲刷屏（性能核心）

没有 GPU 的 MCU 上，刷屏是最大瓶颈。传统写法是 CPU 死循环把像素写进 FSMC：

```c
while (n--) { LCD->LCD_RAM = *p++; }     // CPU 全程被占死
```

本项目改成 **DMA 搬运 + 双缓冲流水线**：

```c
lcd_set_window(...);                 // CPU 设窗口
lcd_write_ram_prepare();             // CPU 发 0x2C
HAL_DMA_Start_IT(&dma, buf, &LCD->LCD_RAM, n);   // 交给 DMA
return;                              // 立刻返回，LVGL 去渲染下一块
```

DMA 传输完成中断里再调 `lv_display_flush_ready()`。于是 **DMA 搬 buf1 的同时 CPU 在画 buf2**，
CPU 占用从 80% 降到 30~40%，滑动从 20~30 fps 提到 40+ fps。

> 前置条件：为了让 DMA 能直接搬像素，必须去掉"逐像素红蓝交换"。
> 做法是关闭面板 MADCTL 的 BGR 位，让 LVGL 的颜色格式与面板一致。

### 2. JPEG 流式解码：16 KB 显示全屏图

`lv_image` 会把整张图解进 RAM（320×240×2 = **150 KB**，这颗芯片给不出）。所以自己写
`photo_view.c`，利用 TJpgDec 的 `jd_decomp` **按 MCU 块回调**的特性：

```
SD 读一段 → 解码出 16 行 → 立刻写 LCD → 继续解码下一段
```

内存只要 **16 KB**（10 KB 行带 + 6 KB 解码工作池），而且这 16 KB 还是**借 LVGL 空闲的
显示缓冲**（看图时 LVGL 暂停），所以静态 RAM 零增量。

**坑**：`jd_decomp` 的 `scale` 参数是**指数**（0=1/1, 1=1/2, 2=1/4, 3=1/8），不是除数。

### 3. 自写 FAT16/FAT32 只读驱动

Keil 自带的是 **RL-FlashFS**（只有 `.lib`，需要整套 RTE 配置，且会接管 C 库的
`fopen/fread`，与本工程的 `printf` 重定向冲突）。所以自己写了只读 FAT 驱动
`fat_ro.c`：支持 MBR/超级软盘、FAT16/32、8.3 短名、按名字读文件、文件句柄（open/read/seek）。

### 4. iOS 风格图标：PC 端预渲染成位图

圆角 + 渐变 + 投影如果让 LVGL 每帧实时算，既慢又不好看（圆角要算 mask、阴影要高斯模糊）。
所以用 `tools/gen_icons.py` 在 PC 上生成 84×84 的不透明 RGB565 位图，
**壁纸渐变和阴影提前烘进图里**，运行时只需一次内存拷贝。

- 内嵌 Flash：14 KB/张 × 6 = 84 KB，**0 RAM**，绘制最快
- 也可把 `ICONS_FROM_SD` 改成 1 从 SD 读（省 Flash，但每帧读卡会明显掉帧）

**坑**：LVGL 的二进制图片解码器**只认小写 `.bin` 扩展名**：

```c
if (lv_strcmp(lv_fs_get_ext(src), "bin")) return LV_RESULT_INVALID;
```

### 5. 触摸校准存内部 Flash

这块板没有 AT24C02，原驱动把校准值存 24C02，读不到就每次上电重新校准。
改为存 MCU 内部 Flash 的 Sector 7（`0x08060000`），并用链接器符号
`Image$$ER_IROM1$$Limit` 做保护：**一旦程序长到盖住那个扇区就拒绝擦写**。

### 6. 其它性能与稳定性措施

| 措施 | 效果 |
|---|---|
| 编译 `-O3` | 渲染 +20~40% |
| FSMC `ADDSET` 9→4 | 每像素 108ns → 78ns（刷屏 +28%） |
| 刷新周期 33→16 ms | 60fps 上限 |
| 显示缓冲 45→32 行 | 省 17 KB RAM 给 LVGL 堆 |
| LVGL 控件裁剪（关掉 28 个控件 + 9 种色深） | Flash 467→258 KB |
| 2048 块改**固定对象池** | 消除每步 32 次申请/释放，修掉堆碎片导致的卡死 |
| 触摸抖动过滤（2 像素滞回）+ `LV_EVENT_PRESSED` | 电阻屏点得准 |

## SD 卡文件约定

根目录放这些文件（8.3 短名）：

| 文件 | 用途 |
|---|---|
| `PIC1.JPG` ~ `PIC4.JPG` | 图片浏览器读取的测试图（320×240） |
| `I2048.bin` 等 6 个 | 可选：从 SD 载入桌面图标（需把小写 `.bin` 扩展名的路径传进去） |

## 已知问题与 TODO

- [ ] 实体按键（KEY0 PE4 / KEY1 PE3 / WK_UP PA0）极性未确认，目前用屏幕 RST 按钮代替
- [ ] TO DO / 设置 / 关于 三个 app 还是占位页
- [ ] 中文界面字体占 Flash 太大，界面暂用英文
- [ ] 文件系统只读（暂不支持写入）
- [ ] MJPEG 视频播放未做（图片浏览已打通，视频只是"自动翻页"）

## 第三方组件

| 组件 | 版本 | 许可 |
|---|---|---|
| LVGL | 9.5.0-dev | MIT |
| TJpgDec (ChaN) | R0.03 | 作者免费授权（见 `src/libs/tjpgd/LICENSE.txt`） |
| STM32F4xx HAL | — | BSD-3-Clause |
| CMSIS | — | Apache-2.0 |

## 许可

本仓库中**本人编写的代码**（`Drivers/BSP/`、`GUI/`、`User/`、`tools/`）采用 MIT 许可。
第三方组件遵循其各自许可。

#ifndef APP_SSD1331_H
#define APP_SSD1331_H

/**
 * SPI経由で96x64 16bit-Color OLEDディスプレイ(SSD1331)を駆動する

   * GPIO 17 (pin 22) Chip select -> SSD1331ボードのCS (Chip Select)
   * GPIO 18 (pin 24) SCK/spi0_sclk -> SSD1331ボードのSCL
   * GPIO 19 (pin 25) MOSI/spi0_tx -> SSD1331ボードのSDA (MOSI)
   * GPIO 21 (pin 27) -> SSD1331ボードのRES (Reset)
   * GPIO 20 (pin 26) -> SSD1331ボードのDC (Data/Command)
   * 3.3V OUT (pin 36) -> SSD1331ボードのVCC
   * GND (pin 38)  -> SSD1331ボードのGND
 */
#define SPI_SCK_PIN         18
#define SPI_MOSI_PIN        19
#define SPI_CSN_PIN         17
#define SPI_DCN_PIN         20
#define SPI_RESN_PIN        21

#define SSD1331_HEIGHT      64
#define SSD1331_WIDTH       96
#define SSD1331_BUF_LEN     (SSD1331_HEIGHT * SSD1331_WIDTH)
#define SSD1331_LBUF_LEN    (8 * SSD1331_WIDTH)

#define RGB(r,g,b)	(UH)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))

#define COL_BLACK	RGB(0,0,0)
#define COL_WHITE	RGB(255,255,255)
#define COL_RED		RGB(255,0,0)
#define COL_GREEN	RGB(0,255,0)
#define COL_BLUE	RGB(0,0,255)

#define COL_YELLOW	RGB(255,255,0)
#define COL_MAGENTA	RGB(255,0,255)
#define COL_AQUA	RGB(0,255,255)

#define COL_PURPLE	RGB(160,32,240)
#define COL_REDPINK RGB(255,50,50)
#define COL_ORANGE  RGB(255,165,0)
#define	COL_LGRAY	RGB(160,160,160)
#define	COL_GRAY	RGB(128,128,128)

#define COL_FRONT   COL_WHITE
#define COL_BACK   	COL_BLACK

// 基本コマンド
#define SSD1331_SET_COL_ADDR        (0x15)    // 桁アドレスの設定: 0x00/0x5F
#define SSD1331_SET_ROW_ADDR        (0x75)    // 行アドレスの設定: 0x00/0x3F
#define SSD1331_SET_CONTRAST_A      (0x81)    // カラー"A"のコントラストの設定: 0x80
#define SSD1331_SET_CONTRAST_B      (0x82)    // カラー"B"のコントラストの設定: 0x80
#define SSD1331_SET_CONTRAST_C      (0x83)    // カラー"C"のコントラストの設定: 0x80
#define SSD1331_MASTER_CURRENT_CNTL (0x87)    // マスターカレント制御: 0x0F
#define SSD1331_SET_PRECHARGE_A     (0x8A)    // カラー"A"の二次プリチャージ速度の設定: 0x81
#define SSD1331_SET_PRECHARGE_B     (0x8B)    // カラー"A"の二次プリチャージ速度の設定: 0x82
#define SSD1331_SET_PRECHARGE_C     (0x8C)    // カラー"A"の二次プリチャージ速度の設定: 0x83
#define SSD1331_REMAP_COLOR_DEPTH   (0xA0)    // Remapとカラー震度の設定: 0x40
#define SSD1331_SET_DISP_START_LINE (0xA1)    // ディスプレイ開始行の設定: 0x00
#define SSD1331_SET_DISP_OFFSET     (0xA2)    // ディスプレイオフセットの設定: 0x00
#define SSD1331_SET_NORM_DISP       (0xA4)
#define SSD1331_SET_ALL_ON          (0xA5)
#define SSD1331_SET_ALL_OFF         (0xA6)
#define SSD1331_SET_INV_DISP        (0xA7)
#define SSD1331_SET_MUX_RATIO       (0xA8)    // 多重度比を設定: 0x3F
#define SSD1331_SET_DIM             (0xAB)    // Dimモードの設定: 0x00/A/B/C/D/プリチャージ
#define SSD1331_SET_MASTER_CONFIG   (0xAD)    // マスター構成の設定: 0x8F
#define SSD1331_SET_DISP_ON_DIM     (0xAC)    // DIMモードでディスプレイオン
#define SSD1331_SET_DISP_OFF        (0xAE)    // ディスプレイオフ
#define SSD1331_SET_DISP_ON_NORM    (0xAF)    // ノーマルモードでディスプレイオン
#define SSD1331_POWER_SAVE          (0xB0)    // パワーセーブモード: 0x1A: 有効、0x0B: 無効
#define SSD1331_ADJUST              (0xB1)    // フェーズ1,2クロック数の調整: 0x74
#define SSD1331_DISP_CLOck          (0xB3)    // ディスプレイクロック分周比/発振周波数: 0xD0
#define SSD1331_SET_GRAY_SCALE      (0xB8)    // グレイスケールテーブルの設定: 32バイト
#define SSD1331_EN_LINEAR_SCALE     (0xB9)    // 線形グレイスケールテーブルの有効化
#define SSD1331_SET_PRECHARGE_LEVEL (0xBB)    // プリチャージレベルの設定: 0x3E
#define SSD1331_NOP                 (0xBC)    // NOP, 0xBD, 0xE3もNOP
#define SSD1331_SET_VCOMH           (0xBE)    // Vcomhの設定: 0x3E
#define SSD1331_SET_COMND_LOCK      (0xFD)    // コマンドロックの設定: 0x12

// 描画コマンド
#define SSD1331_DRAW_LINE          (0x21)     // ラインの描画: 7バイト
#define SSD1331_DRAW_RECT          (0x22)     // 矩形の描画: 10バイト
#define SSD1331_COPY               (0x23)     // コピー: 6バイト
#define SSD1331_DIM_WIN            (0x24)     // ウィンドウをぼやかす: 4バイト
#define SSD1331_CLEAR_WIN          (0x25)     // ウィンドウをクリア: 4バイト
#define SSD1331_FILL               (0x26)     // 塗りつぶしの有効/無効: 0x00
#define SSD1331_SETUP_SCROL        (0x27)     // 水平/垂直スクロールの設定: 5バイト
#define SSD1331_DEACT_SCROL        (0x2E)     // スクロールを停止
#define SSD1331_ACT_SCROL          (0x2F)     // スクロールを開始

#define	SSD1331_CMD_MODE    (0x1)          // SSD1331コマンドモード
#define	SSD1331_DATA_MODE	(0x0)          // SSD1331データモード

typedef enum {
    SPI_CPHA_0 = 0, /*!< 0/1でデータ取り込み */
    SPI_CPHA_1 = 1  /*!< 1/0でデータ取り込み */
} spi_cpha_t;

/** \brief SPI CPOL (クロックの極性) 値の列挙型.
 *  \ingroup hardware_spi
 */
typedef enum {
    SPI_CPOL_0 = 0, /*!< 正論理 */
    SPI_CPOL_1 = 1  /*!< 負論理 */
} spi_cpol_t;

/** \brief SPI ビット順値の列挙型.
 *  \ingroup hardware_spi
 */
typedef enum {
    SPI_LSB_FIRST = 0,  /*!< LSBから */
    SPI_MSB_FIRST = 1   /*!< MSBから */
} spi_order_t;

struct render_area {
    INT start_col;
    INT end_col;
    INT start_row;
    INT end_row;

    INT buflen;
};

// 指定のGPIO端子に1/0を出力する
static inline void gpio_put(UW gpio, BOOL value) {
    UW mask = 1u << gpio;
    if (value) {
        asm volatile("nop \n nop \n nop");
        out_w(GPIO_OUT_SET, mask);
        asm volatile("nop \n nop \n nop");
    }  else {
        asm volatile("nop \n nop \n nop");
        out_w(GPIO_OUT_CLR, mask);
        asm volatile("nop \n nop \n nop");
    }
}

// 指定のGPIO端子の方向を決める. out = TRUE: output、FALSE: input
static inline void gpio_set_output(UW gpio, BOOL out) {
    UW mask = 1u << gpio;
    if (out)
        out_w(GPIO_OE_SET, mask);
    else
        out_w(GPIO_OE_CLR, mask);
}

static inline INT get_font_index(UB ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else if (ch == '.') {
        return 37;
    }
    else if (ch == ':') {
        return 38;
    }
    else if (ch == '-') {
        return 39;
    }
    else return  0; // Not got that char so space.
}

static inline UB to_upper(UB ch) {
    if (ch < 'a' || ch > 'z') return ch;
    return 'A' + ch - 'a';
}

static inline void strncpy(UB *dst, const UB *src, SZ n)
{
  UB *d = dst;

  while (n-- > 0 && (*d++ = *src++));
}

static inline UB strlen(const UB *str) {
    UB len = 0;
    while (*str++) ++len;
    return len;
}

static inline void *memset(void *dst, INT c, SZ n) {
    B *d = dst;
    while (n-- > 0) {
        *d++ = c;
    }
    return dst;
}

#define count_of(a) (sizeof(a)/sizeof((a)[0]))

#endif

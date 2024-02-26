/*
 *    OLED制御タスク
*/

#include <trykernel.h>
#include "app.h"
#include "ssd1331.h"
#include "font.h"

extern void spi_set_format(UW unit, UINT data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t corder);

void task_oled(INT stacd, void *exinf);             // タスクの実行関数
UW  tskstk_oled[32768/sizeof(UW)];                  // タスクのスタック領域
ID  tskid_oled;                                     // タスクID

/* タスク生成情報 */
T_CTSK  ctsk_oled = {
    .tskatr     = TA_HLNG | TA_RNG3 | TA_USERBUF,   // タスク属性
    .task       = task_oled,                        // タスクの実行関数
    .itskpri    = 10,                               // タスク優先度
    .stksz      = sizeof(tskstk_oled),              // スタックサイズ
    .bufptr     = tskstk_oled,                      // スタックへのポインタ
};

static void reset(void) {
    gpio_put(SPI_RESN_PIN, 1);
    gpio_put(SPI_CSN_PIN, 1);
    tk_dly_tsk(5);
    gpio_put(SPI_RESN_PIN, 0);
    tk_dly_tsk(80);
    gpio_put(SPI_RESN_PIN, 1);
    tk_dly_tsk(20);
}

// TODO: unit番号を得る
static void send_cmd(UB cmd) {
    spi_set_format(0, 8, SPI_CPOL_0,  SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(SPI_CSN_PIN, 0);   // チップセレクト
    gpio_put(SPI_DCN_PIN, 0);   // コマンドモード
    tk_swri_dev(dd_spi0, SSD1331_CMD_MODE, &cmd, 1, 0);
    gpio_put(SPI_DCN_PIN, 1);
    gpio_put(SPI_CSN_PIN, 1);   // 連続出力の場合は必須
}

static void send_cmd_list(UB *buf, SZ size) {
    for (INT i = 0; i < size; i++)
        send_cmd(buf[i]);
}

// TODO: spi_set_formatをAPIで実現
static void send_data(UH *buf, SZ size) {
    spi_set_format(0, 16, SPI_CPOL_0,  SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(SPI_CSN_PIN, 0);   // チップセレクト
    gpio_put(SPI_DCN_PIN, 1);   // データモード
    tk_swri_dev(dd_spi0, SSD1331_DATA_MODE, buf, size, 0);
    gpio_put(SPI_DCN_PIN, 0);
    gpio_put(SPI_CSN_PIN, 1);   // 連続出力の場合は必須
}

static void gpio_init(UINT gpio) {
    gpio_set_output(gpio, FALSE);                   // 端子出力無効
    out_w(GPIO_OUT_CLR, (1 << gpio));               // 端子出力クリア
    out_w(GPIO_CTRL(gpio), GPIO_CTRL_FUNCSEL_SIO);  // 端子機能SIO
}

static ER init_oled(void) {
    gpio_put(SPI_CSN_PIN, 1);

    //  CS (Chip Select)ピン: アクティブLOWなので、HIGH状態で初期化
    gpio_init(SPI_CSN_PIN);
    gpio_set_output(SPI_CSN_PIN, TRUE);
    gpio_put(SPI_CSN_PIN, 1);

    // D/C# (Data/Command)ピン: データモードで初期化
    gpio_init(SPI_DCN_PIN);
    gpio_set_output(SPI_DCN_PIN, TRUE);
    gpio_put(SPI_DCN_PIN, 1);

    // RES# (Data/Command)ピン: アクティブLOWなので、HIGH状態で初期化
    gpio_init(SPI_RESN_PIN);
    gpio_set_output(SPI_RESN_PIN, TRUE);
    gpio_put(SPI_RESN_PIN, 1);

    // ssd1331をリセット
    reset();

    // デフォルト値で初期化: Adafruit-SSD1331-OLED-Driver-Library-for-Arduinoから引用
    UB cmds[] = {
        SSD1331_SET_DISP_OFF,
        SSD1331_SET_ROW_ADDR, 0x00, 0x3F,
        SSD1331_SET_COL_ADDR, 0x00, 0x5F,
        SSD1331_REMAP_COLOR_DEPTH, 0x72,
        SSD1331_SET_DISP_START_LINE, 0x00,
        SSD1331_SET_DISP_OFFSET, 0x00,
        SSD1331_SET_NORM_DISP,
        SSD1331_SET_MUX_RATIO, 0x3F,
        SSD1331_SET_MASTER_CONFIG, 0xBE,
        SSD1331_POWER_SAVE, 0x0B,
        SSD1331_ADJUST, 0x74,
        SSD1331_DISP_CLOck, 0xF0,
        SSD1331_SET_PRECHARGE_A, 0x64,
        SSD1331_SET_PRECHARGE_B, 0x78,
        SSD1331_SET_PRECHARGE_C, 0x64,
        SSD1331_SET_PRECHARGE_LEVEL, 0x3A,
        SSD1331_SET_VCOMH, 0x3E,
        SSD1331_MASTER_CURRENT_CNTL, 0x06,
        SSD1331_SET_CONTRAST_A, 0x91,
        SSD1331_SET_CONTRAST_B, 0x50,
        SSD1331_SET_CONTRAST_C, 0x7D,
        SSD1331_SET_DISP_ON_NORM
    };
    send_cmd_list(cmds, count_of(cmds));
    return E_OK;
}

static void render(UH *buf, struct render_area *area) {
    // render_areaでディスプレイの一部を更新
    UB cmds[] = {
        SSD1331_SET_COL_ADDR,
        area->start_col,
        area->end_col,
        SSD1331_SET_ROW_ADDR,
        area->start_row,
        area->end_row
    };

    send_cmd_list(cmds, count_of(cmds));
    send_data(buf, area->buflen);
}

static void set_pixel(UH *buf, INT x, INT y, UH color) {
    INT idx = y * 96 + x;
    buf[idx] = color;
}

static void write_char(UH *buf, INT x, INT y, UB ch, UH color) {
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    UB b, c, p;

    ch = to_upper(ch);
    INT idx = get_font_index(ch);    // 文字のfont[]内の行数
    for (INT i = 0; i < 8; i++) {
        c = font[idx*8+i];
        p = 0x80;
        for (INT k = 0; k < 8; k++) {
            b = (c & p);
            p >>= 1;
            set_pixel(buf, x + k, y + i, b ? color : COL_BLACK);
        }
    }
}

// y行のx桁から文字列strを書き込む
static void write_string(UH *buf, INT x, INT y, UB *str, UH color) {
    // Cull out any string off the screen
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    while (*str) {
        write_char(buf, x, y, *str++, color);
        x += 8;
    }
}

/**
 * @brief 小数点以下2桁の小数を100倍した整数を小数として文字列に変換する
 *
 * @param num 小数点以下2桁の小数を100倍した整数（有効桁数は小数２桁を含み、正数は6桁、負数は5桁。それ以上は上位桁が削除される）
 * @param buf 小数点文字列を格納するバッファ(バッファ長は10とする)
 * @return INT 文字列の長さ
 */
static INT itoa(INT num, UB *buf) {
    UB temp[10];
    INT quo, rem, idx = 0, minus = 0;

    if (num < 0) {
        minus = 1;
        num *= -1;
    }

    do {
        quo = num / 10;
        rem = num - quo * 10;
        temp[idx++] = '0' + rem;
        if (idx == 2) temp[idx++] = '.';
        num = quo;
    } while (num != 0);

    if (minus) temp[idx++] = '-';
    temp[idx] = 0;

    for (INT i = 0; i < idx; i++) {
        buf[i] = temp[idx-1-i];
    }

    return idx;
}

void make_value(UB *buf, const UB *H, INT val) {
    UB tmp[10];

    memset(buf, 0, 12);
    INT len = itoa(val, tmp);
    strncpy(buf, H, 3);
    strncpy(buf+3, tmp, len);
}

/* タスクの実行関数*/
void task_oled(INT stacd, void *exinf) {
    UINT flgptn;
    ER err;

    UH buf[SSD1331_BUF_LEN];        // 表示バッファ
    UB vbuf[12];                    // 測定値用バッファ
    const UB title[] = "ENV DATA";
    const UB PH[] = "P: ";
    const UB TH[] = "T: ";
    const UB HH[] = "H: ";

    err = init_oled();
    if (err == E_OK) tm_putstring("OLED inited\n");

    // 描画領域を初期化
    struct render_area frame_area = {
        start_col: 0,
        end_col : SSD1331_WIDTH - 1,
        start_row : 0,
        end_row : SSD1331_HEIGHT - 1,
        buflen : SSD1331_WIDTH * SSD1331_HEIGHT
    };

    // 画面全体を黒で塗りつぶす
    for (INT i = 0; i < SSD1331_BUF_LEN; i++) {
        buf[i] = COL_BLACK;
    }

    // タイトルを表示
    write_string(buf, 8, 0, title, COL_WHITE);

    while(1) {
        // 環境センサーのデータ表示
        tk_wai_flg(flgid_1, FLG_ALL, TWF_ORW | TWF_BITCLR, &flgptn, TMO_FEVR);  // フラグのセット待ち
        if (flgptn) {
            if (flgptn & FLG_PRES) {
                // 気圧を表示
                make_value(vbuf, PH, pres_data);
                write_string(buf, 0, 10, vbuf, COL_WHITE);
            }
            if (flgptn & FLG_TEMP) {
                // 温度を表示
                make_value(vbuf, TH, temp_data);
                write_string(buf, 0, 20, vbuf, COL_WHITE);
            }
            if (flgptn & FLG_HUMI) {
                // 湿度を表示
                make_value(vbuf, HH, humi_data);
                write_string(buf,  0, 30, vbuf, COL_WHITE);
            }
            render(buf, &frame_area);
        }
    }

}

/*
 *    OLED制御タスク
*/

#include <trykernel.h>
#include "app.h"
#include "ssd1331.h"
#include "font.h"

extern void spi_set_format(UW unit, UINT data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t corder);

void task_oled(INT stacd, void *exinf);             // タスクの実行関数
UW  tskstk_oled[1024/sizeof(UW)];                   // タスクのスタック領域
ID  tskid_oled;                                     // タスクID

/* タスク生成情報 */
T_CTSK  ctsk_oled = {
    .tskatr     = TA_HLNG | TA_RNG3 | TA_USERBUF,   // タスク属性
    .task       = task_oled,                        // タスクの実行関数
    .itskpri    = 10,                               // タスク優先度
    .stksz      = sizeof(tskstk_oled),              // スタックサイズ
    .bufptr     = tskstk_oled,                      // スタックへのポインタ
};

static void set_pixel(UH *buf, INT x, INT y, UH color) {
    INT idx = y * 96 + x;
    buf[idx] = color;
}

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
    //gpio_put(SPI_DCN_PIN, 1);
    gpio_put(SPI_CSN_PIN, 1);   // 連続出力の場合は必須
}

static void send_cmd_list(UB *buf, SZ size) {
    for (INT i = 0; i < size; i++)
        send_cmd(buf[i]);
}

// TODO: unit番号を得る
static void send_data(UH *buf, SZ size) {
    spi_set_format(0, 16, SPI_CPOL_0,  SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_put(SPI_CSN_PIN, 0);   // チップセレクト
    gpio_put(SPI_DCN_PIN, 1);   // データモード
    tk_swri_dev(dd_spi0, SSD1331_DATA_MODE, buf, size, 0);
    //gpio_put(SPI_DCN_PIN, 0);
    gpio_put(SPI_CSN_PIN, 1);   // 連続出力の場合は必須
}

static ER init_oled(void) {
    ER err;
    tm_putstring("int_oled: called\n");
    gpio_put(SPI_CSN_PIN, 1);

    // D/C# (Data/Command)ピン: データモードで初期化
    out_w(GPIO_CTRL(SPI_DCN_PIN), GPIO_CTRL_FUNCSEL_SIO);
    gpio_set_output(SPI_DCN_PIN, TRUE);
    gpio_put(SPI_DCN_PIN, 1);

    // RES# (Data/Command)ピンはアクティブLOWなので、HIGH状態で初期化
    out_w(GPIO_CTRL(SPI_RESN_PIN), GPIO_CTRL_FUNCSEL_SIO);
    gpio_set_output(SPI_RESN_PIN, TRUE);
    gpio_put(SPI_RESN_PIN, 1);

    tm_putstring("int_oled: set pins\n");
    // ssd1331をリセット
    reset();
    tm_putstring("int_oled: reset\n");

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

static void write_string(UH *buf, INT x, INT y, UB *str, UH color) {
    // Cull out any string off the screen
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    while (*str) {
        write_char(buf, x, y, *str++, color);
        x += 8;
    }
}

static inline void strncpy(UB *dst, const UB *src, SZ n)
{
  UB *d = dst;

  while (n-- > 0 && (*d++ = *src++));
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

    for (INT i = 1; i < idx; i++) {
        buf[i] = temp[idx-1-i];
    }

    return idx;
}

/* タスクの実行関数*/
void task_oled(INT stacd, void *exinf) {
    UINT    flgptn;
    ER err;

    UH buf[SSD1331_BUF_LEN];
    UB vbuf[12];
    UB vtmp[10];
    INT vlen;
    UB title[] = "ENV DATA";
    const UB PH[] = "P: ";
    const UB TH[] = "T: ";
    const UB HH[] = "H: ";

    tm_putstring("task_oled started\n");

    err = init_oled();
    if (err < E_OK) tm_putstring("ERROR OLED init\n");

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
    render(buf, &frame_area);
    // タイトルを表示
    write_string(buf, 8, 0, title, COL_WHITE);

    while(1) {
        tk_wai_flg(flgid_1, FLG_ALL, TWF_ORW | TWF_BITCLR, &flgptn, TMO_FEVR);  // フラグのセット待ち

        /* 環境センサーのデータ表示 */
        if (flgptn & FLG_PRES) {
            // 気圧を表示
            vlen = itoa(pres_data, vtmp);
            strncpy(vbuf, PH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(buf, 0, 10, vbuf, COL_WHITE);
        }
        if (flgptn & FLG_TEMP) {
            // 温度を表示
            vlen = itoa(temp_data, vtmp);
            strncpy(vbuf, TH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(buf, 0, 20, vbuf, COL_WHITE);
        }
        if (flgptn & FLG_HUMI) {
            // 湿度を表示
            vlen = itoa(humi_data, vtmp);
            strncpy(vbuf, HH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(buf, 0, 30, vbuf, COL_WHITE);
        }
    }
}

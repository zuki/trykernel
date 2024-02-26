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
    gpio_put(SPI_DCN_PIN, 1);
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

static void set_line_area(struct render_area *area, INT y) {
    area->start_col = 0;
    area->end_col = SSD1331_WIDTH - 1;
    area->start_row = y;
    area->end_row = y + 8 - 1;
    area->buflen = SSD1331_WIDTH * 8;
}

// y行のx桁から文字列strを書き込む
static void write_string(UH *buf, struct render_area *area, INT x, INT y, UB *str, UH color) {
    // Cull out any string off the screen
    if (x > SSD1331_WIDTH - 8 || y > SSD1331_HEIGHT - 8)
        return;

    // バッファをクリア
    memset(buf, 0, sizeof(buf));

    UB *temp_s = str;
    INT temp_x = x;

    while (*temp_s) {
        write_char(buf, temp_x, 0, *temp_s++, color);
        temp_x += 8;
    }

    set_line_area(area, y);
    render(buf, area);
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

    //UH buf[SSD1331_BUF_LEN];
    struct render_area line_area;   // 描画領域
    UH lbuf[SSD1331_WIDTH * 8];      // 1行分のバッファ
    UB vbuf[12];                    // 測定値用バッファ
    UB vtmp[10];
    INT vlen;
    const UB title[] = "ENV DATA";
    const UB PH[] = "P: 1007.41";
    const UB TH[] = "T: 25.91";
    const UB HH[] = "H: 30.04";

    err = init_oled();
    if (err == E_OK) tm_putstring("OLED inited\n");

/*
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
*/
    // タイトルを表示
    //write_string(lbuf, &line_area, 8, 0, title, COL_WHITE);
    write_string(lbuf, &line_area, 0, 10, PH, COL_WHITE);
    write_string(lbuf, &line_area, 0, 20, TH, COL_WHITE);
    write_string(lbuf, &line_area, 0, 30, HH, COL_WHITE);
    //tm_putstring("wrote string\n");

    while(1) {

/*
        // 環境センサーのデータ表示
        tk_wai_flg(flgid_1, FLG_ALL, TWF_ORW | TWF_BITCLR, &flgptn, TMO_FEVR);  // フラグのセット待ち
        if (flgptn & FLG_TEMP) {
            // 気圧を表示
            vlen = itoa(pres_data, vtmp);
            strncpy(vbuf, PH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(lbuf, &line_area, 0, 10, vbuf, COL_WHITE);
        }
        if (flgptn & FLG_TEMP) {
            // 温度を表示
            vlen = itoa(temp_data, vtmp);
            strncpy(vbuf, TH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(lbuf, &line_area, 0, 20, vbuf, COL_WHITE);
        }
        if (flgptn & FLG_HUMI) {
            // 湿度を表示
            vlen = itoa(humi_data, vtmp);
            strncpy(vbuf, HH, 3);
            strncpy(vbuf+3, vtmp, vlen);
            write_string(lbuf, &line_area, 0, 30, vbuf, COL_WHITE);
        }
*/
    }

}

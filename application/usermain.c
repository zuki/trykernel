#include <trykernel.h>
#include "app.h"

ID  dd_i2c0;        // I2Cデバイスディスクリプタ
ID  dd_spi0;        // SPIデバイスディスクリプタ
ID  flgid_1;        // イベントフラグID

/* センサーデータ */
W  pres_data;          // 気圧データ
W  temp_data;          // 温度データ
W  humi_data;          // 湿度データ

int usermain(void)
{
    ER err;
    /* イベントフラグの生成情報 */
    T_CFLG  cflg_1 = {
        .iflgptn = 0,
    };

    tm_putstring("usermain started\n");
    // イベントフラグの生成
    flgid_1 = tk_cre_flg(&cflg_1);
    if(flgid_1 < 0) {
        tm_putstring("Error create flag\n");
        goto sleep_task;
    }
    else tm_putstring("Create flag\n");

    // I/Oデバイスのオープン
    dd_i2c0 = tk_opn_dev((UB*)"iica", TD_UPDATE);
    if(dd_i2c0 < 0) {
        tm_putstring("Error I2C0\n");
        goto oled_task;
    }
    else tm_putstring("Open I2C0\n");

    dd_spi0 = tk_opn_dev((UB*)"spia", TD_WRITE);
    if(dd_spi0 < 0) tm_putstring("Error SPI0\n");
    else tm_putstring("Open SPI0\n");

oled_task:
    // OLED制御タスクの生成・実行
    tskid_oled = tk_cre_tsk(&ctsk_oled);
    if (tskid_oled < 0) {
        tm_puterr((ER)tskid_oled);
        goto sleep_task;
    }
    err = tk_sta_tsk(tskid_oled, 0);
    if (err < 0) tm_puterr(err);

bme_task:
    // BME280 環境センサー制御タスクの生成・実行
    tskid_bme = tk_cre_tsk(&ctsk_bme);
    if (tskid_bme < 0) tm_puterr((ER)tskid_bme);
    else tm_putstring("bme created\n");
    err = tk_sta_tsk(tskid_bme, 0);
    if (err < 0) tm_puterr(err);
    else tm_putstring("bme started\n");

sleep_task:

    tk_slp_tsk(TMO_FEVR);       // 初期タスクを待ち状態に
    return 0;
}

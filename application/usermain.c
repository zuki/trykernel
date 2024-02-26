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
    // イベントフラグの生成情報
    T_CFLG  cflg_1 = {
        .iflgptn = 0,
    };

    //tm_putstring("usermain started\n");
    // イベントフラグの生成
    flgid_1 = tk_cre_flg(&cflg_1);

    // I/Oデバイスのオープン
    dd_i2c0 = tk_opn_dev((UB*)"iica", TD_UPDATE);
    if (dd_i2c0 < 0) tm_putstring("Error I2C0\n");
    //else tm_putstring("Open I2C0\n");

    dd_spi0 = tk_opn_dev((UB*)"spia", TD_WRITE);
    if (dd_spi0 < 0) tm_putstring("Error SPI0\n");
    //else tm_putstring("Open SPI0\n");

    // OLED制御タスクの生成・実行
    tskid_oled = tk_cre_tsk(&ctsk_oled);
    if (tskid_oled < 0) tm_putstring("Error create oled\n");
    err = tk_sta_tsk(tskid_oled, 0);
    //if (err == E_OK) tm_putstring("start oled\n");
    if (err < 0) tm_putstring("Error start oled task\n");

    // BME280 環境センサー制御タスクの生成・実行
    tskid_bme = tk_cre_tsk(&ctsk_bme);
    if (tskid_bme < 0) tm_putstring("Error create bme280\n");
    //if (tskid_bme < 0) tm_puterr((ER)tskid_bme);
    //else tm_putstring("bme created\n");
    err = tk_sta_tsk(tskid_bme, 0);
    //if (err == E_OK) tm_putstring("start bme280\n");
    if (err < 0) tm_putstring("Error start bme280\n");

    tk_slp_tsk(TMO_FEVR);       // 初期タスクを待ち状態に
    return 0;
}

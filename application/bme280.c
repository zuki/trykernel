/*
 *     環境センサーBME280制御タスク
*/
#include <trykernel.h>
#include "app.h"
#include "bme280.h"

void task_bme(INT stacd, void *exinf);          // タスクの実行関数
UW  tskstk_bme[1024/sizeof(UW)];                // タスクのスタック領域
ID  tskid_bme;                                  // タスクID

/* タスク生成情報 */
T_CTSK  ctsk_bme = {
    .tskatr     = TA_HLNG | TA_RNG3 | TA_USERBUF,   // タスク属性
    .task       = task_bme,                         // タスクの実行関数
    .itskpri    = 10,                               // タスク優先度
    .stksz      = sizeof(tskstk_bme),               // スタックサイズ
    .bufptr     = tskstk_bme,                       // スタックへのポインタ
};

/** \brief BME280レジスタのリード
 *
 * \param dd デバイスハンドル
 * \param sadr BME280スレーブアドレス
 * \param radr レジスタアドレス
 * \param data readバッファ
 * \return tk_swri_devの返り値
 */
static ER read_bme280_reg(ID dd, UW sadr, UW radr, UB *data, SZ size) {
    T_I2C_EXEC  exec;
    UB          sbuf;

    sbuf = (UB)radr;
    exec.sadr = sadr;
    exec.sbuf = &sbuf;  // 送信バッファはレジスタアドレス
    exec.rbuf = data;   // 受信バッファ

    return tk_swri_dev(dd, TDN_I2C_EXEC, &exec, size, NULL);
}

/** \brief BME280レジスタへのライト
 *
 * \param dd デバイスハンドル
 * \param sadr BME280スレーブアドレス
 * \param radr レジスタアドレス
 * \param data 書き込むデータ
 * \return tk_swri_devの返り値
 */
static ER write_bme280_reg(ID dd, UW sadr, UW radr, UB data) {
    UB	sbuf[2];

    sbuf[0] = radr;
    sbuf[1] = data;

    return tk_swri_dev(dd, sadr, sbuf, sizeof(sbuf), NULL);
}

/*
static void bme280_reset(void) {
    // reset the device with the power-on-reset procedure
    write_bme280_reg(dd_i2c0, BME280_ADDR, REG_RESET, 0xB6);
}
*/

// 気圧/気温/湿度の較正に使用する高精度な温度を計算する中間関数
static W bme280_convert(W temp, struct bme280_calib_param* params) {
    // データシートに記載されている32ビット固定小数点較正実装を使用する
    W var1, var2;
    var1 = ((((temp >> 3) - ((W)params->dig_t1 << 1))) *
            ((W)params->dig_t2)) >> 11;
    var2 = (((((temp >> 4) - ((W)params->dig_t1)) *
              ((temp >> 4) - ((W)params->dig_t1))) >> 12) *
            ((W)params->dig_t3)) >> 14;

    return var1 + var2;
}

// 原データ（気温）を較正する
static W bme280_convert_temp(W temp, struct bme280_calib_param* params) {
    W t_fine = bme280_convert(temp, params);
    return (t_fine * 5 + 128) >> 8;
}

// 原データ（気圧）を較正する
static W bme280_convert_pressure(W pressure, W temp, struct bme280_calib_param* params) {
    W t_fine = bme280_convert(temp, params);

    W var1, var2;
    UW converted;
    var1 = (((W)t_fine) >> 1) - (W)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((W)params->dig_p6);
    var2 += ((var1 * ((W)params->dig_p5)) << 1);
    var2 = (var2 >> 2) + (((W)params->dig_p4) << 16);
    var1 = (((params->dig_p3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((W)params->dig_p2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((W)params->dig_p1)) >> 15);
    if (var1 == 0) {
        return 0;  // 0除算例外を防ぐ
    }
    converted = (((UW)(((W)1048576) - pressure) - (var2 >> 12))) * 3125;
    if (converted < 0x80000000) {
        converted = (converted << 1) / ((UW)var1);
    } else {
        converted = (converted / (UW)var1) * 2;
    }
    var1 = (((W)params->dig_p9) *
            ((W)(((converted >> 3) * (converted >> 3)) >> 13))) >>
           12;
    var2 = (((W)(converted >> 2)) * ((W)params->dig_p8)) >> 13;
    converted =
        (UW)((W)converted + ((var1 + var2 + params->dig_p7) >> 4));
    return converted;
}

// 原データ（湿度）を較正する
static UW bme280_convert_humidity(W humidity, W temp, struct bme280_calib_param* params) {
    W t_fine = bme280_convert(temp, params);

    W var1;
    var1 = (t_fine - ((W)76800));
    var1 = (((((humidity << 14) - (((W)params->dig_h4) << 20) -
               (((W)params->dig_h5) * var1)) + ((W)16384)) >> 15) *
            (((((((var1 * ((W)params->dig_h6)) >> 10) *
                 (((var1 * ((W)params->dig_h3)) >> 11) +
                  ((W)32768))) >> 10) +
               ((W)2097152)) * ((W)params->dig_h2) + 8192) >> 14));
    var1 = (var1 - (((((var1 >> 15) * (var1 >> 15)) >> 7) * ((W)params->dig_h1)) >> 4));
    var1 = (var1 < 0 ? 0 : var1);
    var1 = (var1 > 419430400 ? 419430400 : var1);

    return (UW)(var1 >> 12);
}

// 原データ取得
static ER bme280_read_raw(ID dd, W* temperature, W* pressure, W* humidity) {
    ER err;
    UB buf[8];

    err = read_bme280_reg(dd, BME280_ADDR, REG_PRESSURE_MSB, buf, 8);
    if (err < E_OK) return err;

    // 読み取った20/16ビットの値を変換用に32ビットの符号付き整数に格納する
    *pressure = (W)(buf[0] << 12) | (buf[1] << 4) | (buf[2] >> 4);
    *temperature = (W)(buf[3] << 12) | (buf[4] << 4) | (buf[5] >> 4);
    *humidity = (W)(buf[6] << 8) | buf[7];

    return E_OK;
}

// 較正パラメタの取得
static ER bme280_get_calib_params(ID dd, struct bme280_calib_param* params) {
    ER err;
    UB buf[NUM_CALIB_PARAMS1] = {0};
    UB buf2[NUM_CALIB_PARAMS2] = {0};

    // read 0x88 - 0xa1
    err = read_bme280_reg(dd, BME280_ADDR, REG_DIG_T1_LSB, buf, NUM_CALIB_PARAMS1);
    if (err < E_OK) return err;

    // store these in a struct for later use
    params->dig_t1 = (UH)(buf[1] << 8) | (UH)buf[0];
    params->dig_t2 = (H)(buf[3] << 8)  | (H)buf[2];
    params->dig_t3 = (H)(buf[5] << 8)  | (H)buf[4];

    params->dig_p1 = (UH)(buf[7] << 8) | (UH)buf[6];
    params->dig_p2 = (H)(buf[9] << 8)  | (H)buf[8];
    params->dig_p3 = (H)(buf[11] << 8) | (H)buf[10];
    params->dig_p4 = (H)(buf[13] << 8) | (H)buf[12];
    params->dig_p5 = (H)(buf[15] << 8) | (H)buf[14];
    params->dig_p6 = (H)(buf[17] << 8) | (H)buf[16];
    params->dig_p7 = (H)(buf[19] << 8) | (H)buf[18];
    params->dig_p8 = (H)(buf[21] << 8) | (H)buf[20];
    params->dig_p9 = (H)(buf[23] << 8) | (H)buf[22];

    params->dig_h1 = (UB)buf[25];

    // read 0xe1 - 0xe6
    err = read_bme280_reg(dd, BME280_ADDR, REG_DIG_H2_LSB, buf2, NUM_CALIB_PARAMS2);
    if (err < E_OK) return err;

    params->dig_h2 = (H)(buf2[1] << 8) | (H)buf2[0];
    params->dig_h3 = (UB)buf2[2];
    params->dig_h4 = (H)(buf2[3] << 4) | (H)(buf2[4] & 0x0f);
    params->dig_h5 = (H)(buf2[5] << 4) | (H)(buf2[4] >> 4 & 0x0f);
    params->dig_h6 = (B)buf2[6];

    return E_OK;
}

static ER bme280_init(ID dd, struct bme280_calib_param *params) {
    ER err;

    // 500ms sampling time, x16 filter = 0x90
    UB reg_config_val = ((0x04 << 5) | (0x04 << 2)) & 0xFC;
    err = write_bme280_reg(dd, BME280_ADDR, REG_CONFIG, reg_config_val);
    if (err < E_OK) return err;

    // osrs_h x1 = 0x01
    const UB reg_ctrl_hum_val = 0x01;
    err = write_bme280_reg(dd, BME280_ADDR, REG_CTRL_HUM, reg_ctrl_hum_val);
    if (err < E_OK) return err;

    // osrs_t x1, osrs_p x1, normal mode operation = 0x27
    UB reg_ctrl_meas_val = (0x01 << 5) | (0x01 << 2) | (0x03);
    err = write_bme280_reg(dd, BME280_ADDR, REG_CTRL_MEAS, reg_ctrl_meas_val);
    if (err < E_OK) return err;

    // 較正パラメタを取得する
    err =  bme280_get_calib_params(dd, params);
    tk_dly_tsk(250);    // データポーリングとレジスタ更新がぶつからないようにスリープさせる

    return err;
}

// valueをbaseで割り、小数点以下2桁までを100倍して返す
static W calc_percentage(W value, W base) {
    W quo, rem, ret, minus = 0;

    if (value < 0) {
        minus = 1;
        value *= -1;
    }
    quo = value / base;
    rem = value - quo * base;
    // 小数点3桁以下は切り捨てる
    if (rem > 1000) rem /= 100;
    else if (rem > 100) rem /= 10;

    ret = quo * 100 + rem;
    if (minus) ret *= -1;
    return ret;
}

/* タスクの実行関数*/
void task_bme(INT stacd, void *exinf)
{
    // 生データ、前データ
    W raw_temp, cnv_temp, pre_temp;
    W raw_pres, cnv_pres, pre_pres;
    W raw_humi, cnv_humi, pre_humi;
    UINT tflg;
    ER  err;
    struct bme280_calib_param params;   // 較正パラメタ

    err = bme280_init(dd_i2c0, &params);               // 環境センサーBME280の初期化
    //if (err < E_OK) tm_putstring("ERROR BME280 init\n");

    pres_data = pre_pres = temp_data = pre_temp = humi_data = pre_humi = 0;
    while (1) {
        tflg = 0;
        // センサーデータの読み取り
        err = bme280_read_raw(dd_i2c0, &raw_temp, &raw_pres, &raw_humi);
        if (err >= E_OK) {
            cnv_temp = bme280_convert_temp(raw_temp, &params);
            cnv_pres = bme280_convert_pressure(raw_pres, raw_temp, &params);
            cnv_humi = bme280_convert_humidity(raw_humi, raw_temp, &params);
            if (cnv_pres != pre_pres) {
                pres_data = calc_percentage(cnv_pres, 100);
                tflg += 1;      // イベントフラグのセット
                pre_temp = cnv_pres;
            }
            if (cnv_temp != pre_temp) {
                temp_data = calc_percentage(cnv_temp, 100);
                tflg +=2 ;       // イベントフラグのセット
                pre_temp = cnv_temp;
            }
            if (cnv_humi != pre_humi) {
                humi_data = calc_percentage(cnv_humi, 1024);
                tflg += 4;       // イベントフラグのセット
                pre_humi = cnv_humi;
            }
            if (tflg) tk_set_flg(flgid_1, FLG_BME);    // いずれかの値が変更されたらフラグセット
        } else {
            tm_putstring("ERROR Read BME280\n");
        }
        tk_dly_tsk(1000);               // 1秒おきに測定
    }
}

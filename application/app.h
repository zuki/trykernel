extern ID  dd_i2c0, dd_i2c1;   // I2Cデバイスディスクリプタ
extern ID  dd_spi0, dd_spi1;   // SPIデバイスディスクリプタ
extern ID  flgid_1;            // イベントフラグID

/* センサーデータ */
extern W  pres_data;          // 気圧データ
extern W  temp_data;          // 気温データ
extern W  humi_data;          // 湿度データ

/* イベントフラグのフラグ定義 */
#define FLG_PRES    (1<<0)      // 気圧データ更新
#define FLG_TEMP    (1<<1)      // 気温データ更新
#define FLG_HUMI    (1<<2)      // 湿度データ更新
#define FLG_ALL     (FLG_PRES|FLG_TEMP|FLG_HUMI)

/* OLED制御タスクIDおよび生成情報 */
extern ID  tskid_oled;
extern T_CTSK  ctsk_oled;

/* BME280制御タスクIDおよび生成情報 */
extern ID  tskid_bme;
extern T_CTSK  ctsk_bme;

extern ID  dd_i2c0, dd_i2c1;   // I2Cデバイスディスクリプタ
extern ID  dd_spi0, dd_spi1;   // SPIデバイスディスクリプタ
extern ID  flgid_1;            // イベントフラグID

/* センサーデータ */
extern W  pres_data;          // 気圧データ
extern W  temp_data;          // 気温データ
extern W  humi_data;          // 湿度データ

/* イベントフラグのフラグ定義 */
#define FLG_GSNS    (1<<0)      // ジェスチャセンサーのデータ更新
#define FLG_LSNS    (1<<1)      // 光センサーのデータ更新
#define FLG_BME     (1<<2)
#define FLG_ALL     (FLG_GSNS|FLG_LSNS|FLG_BME)

/* OLED制御タスクIDおよび生成情報 */
extern ID  tskid_oled;
extern T_CTSK  ctsk_oled;

/* BME280制御タスクIDおよび生成情報 */
extern ID  tskid_bme;
extern T_CTSK  ctsk_bme;

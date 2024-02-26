#include <trykernel.h>
#include "dev_spi.h"
#include "spi_sysdep.h"

// レジスタのベースアドレス
const static UW ba[DEV_SPI_UNITNM] = { SPI0_BASE, SPI1_BASE };

// デバイス制御データ
typedef struct {
    ID      semid;      /* セマフォID */
    UW      init;       /* 初期化フラグ */
    UINT    omode;      /* オープンモード */
} T_SPI_LLDCB;
static T_SPI_LLDCB	ll_devcb[DEV_SPI_UNITNM];

/* ドライバ状態 */
#define	SPI_STS_START       0x0000
#define	SPI_STS_RESTART     0x0001
#define	SPI_STS_STOP        0x0003
#define	SPI_STS_SEND        0x0004
#define	SPI_STS_RECV        0x0005
#define	SPI_STS_TOP	        0x8000


/*
static void spi_set_baudrate(UW unit) {

    clr_w(SPI_CR1(unit), SPI_CR1_SSE);
    out_w(SPI_CPSR(unit), 2);
    set_w(SPI_CR0(unit), (6 << 8));
    set_w(SPI_CR1(unit), SPI_CR1_SSE);
}
*/

static void spi_set_baudrate(UW unit, UW baudrate) {
    // TODO: clk_periの周波数を取得
    //UW freq_in = clock_get_hz(CLK_KIND_PERI);
    UW freq_in = 125 * 1000 * 1000;     // 125 MHz
    UW prescale, postdiv;

    // SPIを無効に
    clr_w(SPI_CR1(unit), SPI_CR1_SSE);

    // 出力周波数がポストディバイドの範囲に入る最小の
    // プリスケール値を求める。プリスケール値は2から254までの偶数
    for (prescale = 2; prescale <= 254; prescale += 2) {
        if (freq_in < (prescale + 2) * 256 * (UD) baudrate)
            break;
    }

    // 出力がbaudate以下となる最大のポストディバイドを求める。
    // ポストディバイドは1から256までの整数
    for (postdiv = 256; postdiv > 1; --postdiv) {
        if (freq_in / (prescale * (postdiv - 1)) > baudrate)
            break;
    }
    // baudrate = 10MHzの場合: prescale = 2, postdiv = 7
    out_w(SPI_CPSR(unit), prescale);
    set_w(SPI_CR0(unit), (((postdiv - 1) & 0xff) << 8));

    // SPIを有効にする
    set_w(SPI_CR1(unit), SPI_CR1_SSE);
}

void spi_set_format(UW unit, UINT data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t corder) {
    // SPIを無効に
    clr_w(SPI_CR1(unit), SPI_CR1_SSE);
    set_w(SPI_CR0(unit), ((UW)(data_bits - 1)) |
                    ((UW)cpol) << 6 |
                    ((UW)cpha) << 7);

    // SPIを最有効化
    set_w(SPI_CR1(unit), SPI_CR1_SSE);
}

/*----------------------------------------------------------------------
 * デバイス初期化
 */
static void spi_init(UW unit) {
    switch(unit) {
    case 0:
        // SPIのリセット
        set_w(RESETS_RESET, RESETS_RESET_SPI0);
        clr_w(RESETS_RESET, RESETS_RESET_SPI0);
        while (!(in_w(RESETS_RESET_DONE) & RESETS_RESET_SPI0));

        // SPI SCKピンの設定
        out_w(GPIO_CTRL(SPI0_SCK), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI0_SCK), GPIO_DRIVE_4MA | GPIO_SHEMITT);
        // SPI TX (MOSI) ピンの設定
        out_w(GPIO_CTRL(SPI0_TX), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI0_TX), GPIO_DRIVE_4MA | GPIO_SHEMITT);
        // SPI RX (MISO) ピンの設定
        out_w(GPIO_CTRL(SPI0_RX), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI0_RX), GPIO_OD | GPIO_IE | GPIO_DRIVE_4MA | GPIO_SHEMITT);

        // SPIの有効化
        set_w(SPI_CR1(unit), SPI_CR1_SSE);
        break;
    case 1:
        // SPIのリセット
        set_w(RESETS_RESET, RESETS_RESET_SPI1);
        clr_w(RESETS_RESET, RESETS_RESET_SPI1);
        while (!(in_w(RESETS_RESET_DONE) & RESETS_RESET_SPI1));
        // SPI SCKピンの設定
        out_w(GPIO_CTRL(SPI1_SCK), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI1_SCK), GPIO_DRIVE_4MA | GPIO_SHEMITT);
        // SPI TX (MOSI) ピンの設定
        out_w(GPIO_CTRL(SPI1_TX), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI1_TX), GPIO_DRIVE_4MA | GPIO_SHEMITT);
        // SPI RX (MISO) ピンの設定
        out_w(GPIO_CTRL(SPI1_RX), GPIO_CTRL_FUNCSEL_SPI);
        out_w(GPIO(SPI1_RX), GPIO_OD | GPIO_IE | GPIO_DRIVE_4MA | GPIO_SHEMITT);

        // SPIの有効化
        set_w(SPI_CR1(unit), SPI_CR1_SSE);
        break;
    }

    out_w(SPI_CR1(unit), 0);     // SPIモジュール無効
}

static inline BOOL spi_is_writable(UW unit) {
    return (in_w(SPI_SR(unit)) & SPI_SR_TNF);
}

static inline BOOL spi_is_readable(UW unit) {
    return (in_w(SPI_SR(unit)) & SPI_SR_RNE);
}

// srcからSPIに直接lenバイト書き出す。SPIから受診したデータは破棄する
//   pico-sdkのspi_write_blocking()を書き換え
static SZ spi_write_blocking(UW unit, const UB *src, SZ len) {
    // TX FIFOに書き込むがRXは無視する。その後クリーンアップする。
    // RXがフルの場合、PL022はRXプッシュを禁止し、プッシュオンフルの
    // スティッキーフラグを設定するがシフトは続行する。SSPIMSC_RORIMが
    // 設定されていなければ安全である
    for (INT i = 0; i < len; ++i) {
        while (!spi_is_writable(unit));
        out_w(SPI_DR(unit), (UW)src[i]);
    }
    // RX FIFOをドレインし、シフトが終了するのを待ち、
    // RX FIFOを再度ドレインする。
    while (spi_is_readable(unit)) (void)in_w(SPI_DR(unit));
    while (SPI_SR(unit) & SPI_SR_BSY);
    while (spi_is_readable(unit)) (void)in_w(SPI_DR(unit));

    // オーバーランフラグはセットしたままにしない
    clr_w(SPI_ICR(unit), SPI_ICR_RORIC);

    return (SZ)len;
}

// srcからSPIに16ビットデータをlen個書き出す。SPIから受信したデータは破棄する
//   pico-sdkのspi_write16_blocking()を書き換え
static SZ spi_write16_blocking(UW unit, const UH *src, SZ len) {
    for (INT i = 0; i < len; ++i) {
        while (!spi_is_writable(unit));
        out_w(SPI_DR(unit), (UW)src[i]);
    }

    while (spi_is_readable(unit)) (void)in_w(SPI_DR(unit));
    while (SPI_SR(unit) & SPI_SR_BSY);
    while (spi_is_readable(unit)) (void)in_w(SPI_DR(unit));

    // オーバーランフラグはセットしたままにしない
    clr_w(SPI_ICR(unit), SPI_ICR_RORIC);

    return (SZ)len;
}

// デバイスオープン関数
ER dev_spi_open(UW unit, UINT omode) {
    T_CSEM  csem = {
        .isemcnt    = 1,
        .maxsem     = 1,
    };
    ER  err;

    if (unit > 1) return E_PAR;

    if (ll_devcb[unit].init == FALSE) {
        err = tk_cre_sem(&csem);       // 排他制御用のセマフォ生成
        if (err < E_OK) return err;

        spi_init(unit);
        ll_devcb[unit].semid = (ID)err;
        ll_devcb[unit].omode = omode;
        ll_devcb[unit].init = TRUE;

        out_w(SPI_CR1(unit), 0);     // SPI(unit): マスターで無効
        spi_set_baudrate(unit, 10*1000*1000);   // TODO: freqのパラメタ化
        spi_set_format(unit, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        // Always enable DREQ signals -- harmless if DMA is not listening
        set_w(SPI_DMACR(unit), SPI_DMACR_TXDMAE | SPI_DMACR_RXDMAE);
    }
    return E_OK;
}

// デバイスライト関数
ER dev_spi_write(UW unit, UW cmd, void *buf, SZ size, SZ *asize) {
    SZ ret;
    if (cmd == 1) {
        ret = spi_write_blocking(unit, (UB *)buf, size);
        if (asize) *asize = ret;
    } else {
        ret = spi_write16_blocking(unit, (UH *)buf, size);
        if (asize) *asize = ret;
    }
    return E_OK;
}
